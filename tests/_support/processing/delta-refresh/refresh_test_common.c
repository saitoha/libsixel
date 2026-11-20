/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/delta-residual.h"
#include "src/bluenoise_64x64.h"

/* Keep assertions local when test support units are amalgamated. */
#define DR_REQUIRE(x) do { if (!(x)) { \
    fprintf(stderr, "refresh %d line %d: %s (%s)\n", mode, __LINE__, #x, \
        sixel_helper_get_additional_message()); goto end; } } while (0)

typedef struct refresh_output {
    unsigned char bytes[65536];
    size_t size;
    size_t painted;
    int fail;
} refresh_output_t;

static int
refresh_write(char *data, int size, void *opaque)
{
    refresh_output_t *out;

    out = opaque;
    if (out->fail || size < 0 || (size_t)size > sizeof(out->bytes) - out->size)
        return -1;
    memcpy(out->bytes + out->size, data, (size_t)size);
    out->size += (size_t)size;
    return size;
}

static void
refresh_done(sixel_playback_result_t const *result, void *opaque)
{
    refresh_output_t *out;

    out = opaque;
    out->painted = result->painted_pixels;
}

/* Model the owner's successful-publication boundary for scheduler tests. */
static void
refresh_commit(sixel_delta_residual_t *state)
{
    state->width = state->mask.width;
    state->height = state->mask.height;
    state->refresh_frame = state->refresh_next_frame;
    state->refresh_origin_x = state->refresh_next_origin_x;
    state->refresh_origin_y = state->refresh_next_origin_y;
    state->refresh_valid = state->refresh_next_valid;
}

/* Encode three slightly changing frames with one update policy. */
static int
refresh_encode_sequence(char const *diffuse, char const *gpu,
    char const *policy, refresh_output_t *out)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    unsigned char pixels[64 * 48 * 3];
    int frame_no, x, y, change, value, channel, ok;

    ok = 0;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, diffuse)
                != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY, gpu)
                != SIXEL_OK ||
            /* The GPU median cut differs from the CPU one; k-center keeps
             * the palette identical so only the refresh path is compared. */
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_QUANTIZE_MODEL,
                "center") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY, policy)
                != SIXEL_OK) {
        fprintf(stderr, "refresh dither %s/%s/%s rejected: %s\n", diffuse,
            gpu, policy, sixel_helper_get_additional_message());
        goto end;
    }
    for (frame_no = 0; frame_no < 3; ++frame_no) {
        /* Changes stay within T8 so that only refresh can repaint them. */
        for (y = 0; y < 48; ++y) {
            for (x = 0; x < 64; ++x) {
                change = frame_no ? (x * 7 + y * 13 + frame_no * 5) % 7 - 3
                    : 0;
                for (channel = 0; channel < 3; ++channel) {
                    value = (channel == 0 ? x * 4 : channel == 1 ? y * 5
                        : (x + y) * 2) + (channel == 1 ? -change : change);
                    pixels[(y * 64 + x) * 3 + channel] = (unsigned char)
                        (value < 0 ? 0 : value > 255 ? 255 : value);
                }
            }
        }
        out[frame_no].size = 0;
        if (sixel_output_new(&output, refresh_write, &out[frame_no], NULL)
                != SIXEL_OK ||
                sixel_frame_new(&frame, NULL) != SIXEL_OK ||
                sixel_frame_init_borrowed(frame, pixels, 64, 48,
                    SIXEL_PIXELFORMAT_RGB888, NULL, 0) != SIXEL_OK ||
                sixel_encoder_encode_frame(encoder, frame, output)
                    != SIXEL_OK) {
            fprintf(stderr, "refresh dither %s/%s/%s frame %d: %s\n",
                diffuse, gpu, policy, frame_no,
                sixel_helper_get_additional_message());
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
        sixel_output_unref(output);
        output = NULL;
    }
    ok = 1;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

/* refresh=1 selects every pixel on every frame, so each early-keep gate
 * must then behave exactly as T0. Any application path that ignored the
 * refresh mask would keep T8 approximations and change the bytes. */
int
test_refresh_dither_equivalence(char const *method, char const *gpu)
{
    static refresh_output_t refreshed[3], exact[3];
    int frame_no;

    if (!refresh_encode_sequence(method, gpu,
            "delta:T8:refresh=1", refreshed) ||
            !refresh_encode_sequence(method, gpu, "delta:T0", exact)) {
        return 1;
    }
    for (frame_no = 0; frame_no < 3; ++frame_no) {
        if (refreshed[frame_no].size != exact[frame_no].size ||
                memcmp(refreshed[frame_no].bytes, exact[frame_no].bytes,
                    exact[frame_no].size) != 0) {
            fprintf(stderr, "refresh %s/%s frame %d differs from T0\n",
                method, gpu, frame_no);
            return 1;
        }
    }
    return 0;
}

int
test_delta_refresh(int mode)
{
    sixel_delta_residual_t state;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    sixel_frame_t *frame;
    refresh_output_t out;
    unsigned char pixels[130 * 77 * 3];
    size_t const paint_counts[] = {10010U, 2497U, 2502U, 2493U, 2518U};
    uint32_t seen[128];
    uint32_t cycle[4][128];
    unsigned char canary[sizeof(sixel_frame_t *)];
    size_t expected, changed, i, legacy_size;
    unsigned int tick, rank;
    int ok;
    SIXELSTATUS status;

    ok = 0;
    encoder = NULL;
    output = NULL;
    playback = NULL;
    frame = NULL;
    memset(&state, 0, sizeof(state));
    memset(&out, 0, sizeof(out));
    memset(seen, 0, sizeof(seen));
    if (mode >= 2 && mode <= 5) {
        /* A restart without valid history selects nothing; frame zero. */
        sixel_delta_refresh_prepare(&state, 128, 128, 0, 0, 4, 0);
        DR_REQUIRE(state.selected_pixels == 0U);
        DR_REQUIRE(state.refresh_next_frame == 0U);
        refresh_commit(&state);
        if (mode == 2) {
            for (tick = 1U; tick <= 4U; ++tick) {
                sixel_delta_refresh_prepare(&state, 128, 128, 0, 0, 4, 1);
                DR_REQUIRE(state.refresh_next_frame == tick);
                DR_REQUIRE(state.selected_pixels == 4096U);
                for (i = 0; i < 128U; ++i) {
                    DR_REQUIRE((seen[i] & state.mask.cells[i]) == 0U);
                    seen[i] |= state.mask.cells[i];
                }
                refresh_commit(&state);
            }
            for (i = 0; i < 128U; ++i) DR_REQUIRE(seen[i] == UINT32_MAX);
        } else if (mode == 3) {
            /* The second period repeats the first period's phase order. */
            for (tick = 1U; tick <= 8U; ++tick) {
                sixel_delta_refresh_prepare(&state, 128, 128, 0, 0, 4, 1);
                DR_REQUIRE(state.selected_pixels == 4096U);
                if (tick <= 4U)
                    memcpy(cycle[tick - 1U], state.mask.cells,
                        sizeof(cycle[0]));
                else
                    DR_REQUIRE(memcmp(cycle[tick - 5U], state.mask.cells,
                        sizeof(cycle[0])) == 0);
                refresh_commit(&state);
            }
        } else if (mode == 4) {
            /* An uncommitted frame does not advance the count. */
            sixel_delta_refresh_prepare(&state, 128, 128, 0, 0, 4, 1);
            memcpy(cycle[0], state.mask.cells, sizeof(cycle[0]));
            sixel_delta_refresh_prepare(&state, 128, 128, 0, 0, 4, 1);
            DR_REQUIRE(state.refresh_next_frame == 1U);
            DR_REQUIRE(state.selected_pixels == 4096U);
            DR_REQUIRE(memcmp(cycle[0], state.mask.cells,
                sizeof(cycle[0])) == 0);
        } else {
            sixel_delta_refresh_prepare(&state, 128, 128, 0, 0, 4, 1);
            refresh_commit(&state);
            /* Moving the rectangle restarts against valid history. */
            sixel_delta_refresh_prepare(&state, 128, 128, 1, 0, 4, 1);
            DR_REQUIRE(state.refresh_next_frame == 0U);
            DR_REQUIRE(state.selected_pixels == 16384U);
            refresh_commit(&state);
            sixel_delta_refresh_prepare(&state, 128, 128, 1, 0, 4, 1);
            DR_REQUIRE(state.selected_pixels == 4096U);
        }
        ok = 1;
        goto end;
    }
    DR_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
        "bluenoise") == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_LUT_POLICY,
        "none") == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        mode == 7 ? "force" : "off") == SIXEL_OK);
    if (mode == 1) {
        DR_REQUIRE(encoder->refresh_frames == 0U);
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T8:refresh=4") == SIXEL_OK);
        DR_REQUIRE(encoder->refresh_frames == 4U);
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T8:J4") == SIXEL_OK);
        DR_REQUIRE(encoder->refresh_frames == 4U);
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T8:J10001") != SIXEL_OK);
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T8:J4:R1") != SIXEL_OK);
        /* Refresh applies to every dither, including error diffusion. */
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
            "fs") == SIXEL_OK);
        DR_REQUIRE(encoder->refresh_frames == 4U);
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta") == SIXEL_OK);
        DR_REQUIRE(encoder->refresh_frames == 0U);
        ok = 1;
        goto end;
    }
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta:T8:J4") == SIXEL_OK);
    DR_REQUIRE(sixel_output_new(&output, refresh_write, &out, NULL)
        == SIXEL_OK);
    DR_REQUIRE(sixel_playback_config_init(&config, sizeof(config)) == SIXEL_OK);
    config.capacity = 2;
    config.done = refresh_done;
    config.done_context = &out;
    if (mode == 10) config.palette_mode = SIXEL_PLAYBACK_PALETTE_LATEST;
    DR_REQUIRE(sixel_playback_new(&playback, encoder, output, &config, NULL)
        == SIXEL_OK);
    DR_REQUIRE(sixel_frame_new(&frame, NULL) == SIXEL_OK);
    memset(pixels, 102, sizeof(pixels));
    DR_REQUIRE(sixel_frame_init_borrowed(frame, pixels, 130, 77,
        SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
    if (mode == 8) {
        /* The pre-palette-source size leaves the appended field alone. */
        legacy_size = offsetof(sixel_playback_options_t, palette_source);
        memset(&options, 0xa5, sizeof(options));
        memset(canary, 0xa5, sizeof(canary));
        DR_REQUIRE(sixel_playback_options_init(&options, legacy_size)
            == SIXEL_OK);
        DR_REQUIRE(options.struct_size == legacy_size);
        DR_REQUIRE(memcmp(&options.palette_source, canary,
            sizeof(canary)) == 0);
        DR_REQUIRE(sixel_playback_submit(playback, frame,
            SIXEL_PLAYBACK_WAIT, -1, &options) == SIXEL_OK);
        DR_REQUIRE(sixel_playback_drain(playback, -1) == SIXEL_OK);
        ok = 1;
        goto end;
    }
    DR_REQUIRE(sixel_playback_options_init(&options, sizeof(options))
        == SIXEL_OK);
    for (tick = 0U; tick <= 4U; ++tick) {
        memset(pixels, tick ? 108 : 102, sizeof(pixels));
        out.size = 0;
        if (mode == 9 && tick == 1U) out.fail = 1;
        status = sixel_playback_submit(playback, frame,
            SIXEL_PLAYBACK_WAIT, -1, &options);
        if (status == SIXEL_OK) status = sixel_playback_drain(playback, -1);
        if (mode == 9 && tick == 1U) {
            DR_REQUIRE(status != SIXEL_OK);
            DR_REQUIRE(!encoder->accumulation_valid);
            DR_REQUIRE(!encoder->residual_state.refresh_valid);
            ok = 1;
            goto end;
        }
        DR_REQUIRE(status == SIXEL_OK);
        expected = paint_counts[tick];
        DR_REQUIRE(out.painted == expected);
        DR_REQUIRE(encoder->residual_state.selected_pixels ==
            (tick ? expected : 0U));
        DR_REQUIRE(encoder->residual_state.refresh_frame == tick);
        changed = 0U;
        for (i = 0; i < 10010U; ++i) {
            /* Check positions as well as counts: a scaled tile has the same
             * histogram but leaves coherent blocks in a uniform fade. */
            rank = sixel_bn64[(((i / 130U) + 17U) & 63U) * 64U +
                              (((i % 130U) + 37U) & 63U)];
            expected = rank < tick * 64U ? 107U : 102U;
            DR_REQUIRE(encoder->accumulation_pixels[i * 3U] == expected);
            if (encoder->accumulation_pixels[i * 3U] != 102) {
                DR_REQUIRE(encoder->accumulation_pixels[i * 3U] == 107);
                ++changed;
            }
        }
        expected = 0U;
        for (i = 1U; i <= tick; ++i) expected += paint_counts[i];
        DR_REQUIRE(changed == expected);
    }
    ok = 1;
end:
    sixel_playback_unref(playback);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? 0 : 1;
}

/* Test unequal ratios and actual RGB8 output using the public setter.
 * Each threshold keeps the original color until its staggered deadline. */
int
test_delta_refresh_periods(void)
{
    static unsigned int const periods[4] = {12U, 6U, 2U, 1U};
    static unsigned int const invalid[4] = {10U, 6U, 2U, 1U};
    static unsigned char const target[4] = {108U, 117U, 122U, 138U};
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    refresh_output_t out;
    unsigned char pixels[64 * 64 * 3];
    size_t painted[4];
    unsigned int band, i, tick;
    int mode, ok;

    mode = 100;
    ok = 0;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    memset(&out, 0, sizeof(out));
    DR_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
        "none") == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        "off") == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta:T48:L4:J1") == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_set_6delta_refresh_periods(encoder, periods)
        == SIXEL_BAD_ARGUMENT);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta:T48:L4:J0:A0") == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_set_6delta_refresh_periods(encoder, periods)
        == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_set_6delta_refresh_periods(encoder, invalid)
        == SIXEL_BAD_ARGUMENT);
    DR_REQUIRE(sixel_output_new(&output, refresh_write, &out, NULL)
        == SIXEL_OK);
    DR_REQUIRE(sixel_frame_new(&frame, NULL) == SIXEL_OK);
    for (band = 0U; band < 4U; ++band) {
        DR_REQUIRE(sixel_encoder_invalidate_6delta_plane(encoder) == SIXEL_OK);
        painted[band] = 0;
        for (tick = 0U; tick <= periods[band]; ++tick) {
            memset(pixels, tick == 0U ? 102 : target[band], sizeof(pixels));
            out.size = 0;
            DR_REQUIRE(sixel_frame_init_borrowed(frame, pixels, 64, 64,
                SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
            DR_REQUIRE(sixel_encoder_encode_frame(encoder, frame, output)
                == SIXEL_OK);
            if (tick == 0U) continue;
            if (tick == 1U && band < 2U)
                DR_REQUIRE(sixel_encoder_get_6delta_painted_pixels(encoder)
                    == (band == 0U ? 384U : 704U));
            painted[band] += sixel_encoder_get_6delta_painted_pixels(encoder);
            for (i = 0; i < 4096U; ++i) {
                if (tick == periods[band])
                    DR_REQUIRE(encoder->accumulation_pixels[i * 3U] != 102U);
            }
        }
        DR_REQUIRE(painted[band] == 4096U);
    }
    DR_REQUIRE(sixel_encoder_set_6delta_refresh_periods(encoder, NULL)
        == SIXEL_OK);
    DR_REQUIRE(encoder->refresh_periods[3] == 0U);
    ok = 1;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? 0 : 1;
}

/* A constant error above the floor must have exactly the advertised period.
 * Literal periods supplied by separate cases are the oracle; the test never
 * derives expectations from scheduler phases or its multiplier table. */
int
test_refresh_adaptive_cadence(unsigned int curve, unsigned int error,
    unsigned int floor, unsigned int period)
{
    sixel_delta_residual_t state;
    unsigned int hits[16 * 16], last[16 * 16];
    unsigned int tick, x, y, pos, threshold;
    int mode, ok;

    mode = (int)error;
    ok = 0;
    memset(&state, 0, sizeof(state));
    memset(hits, 0, sizeof(hits));
    memset(last, 0, sizeof(last));
    sixel_delta_refresh_prepare(&state, 16, 16, 7, 11, 5, 0);
    refresh_commit(&state);
    for (tick = 1; tick <= 640; ++tick) {
        sixel_delta_refresh_prepare(&state, 16, 16, 7, 11, 5, 1);
        sixel_delta_refresh_adapt(&state, 7, 11, 5, floor, curve, NULL);
        for (y = 0; y < 16; ++y) {
            for (x = 0; x < 16; ++x) {
                pos = y * 16 + x;
                threshold = sixel_delta_mask_threshold(&state.mask,
                    (int)x, (int)y, 48);
                if (error <= threshold) continue;
                DR_REQUIRE(period != 0);
                if (last[pos]) DR_REQUIRE(tick - last[pos] == period);
                last[pos] = tick;
                ++hits[pos];
            }
        }
        refresh_commit(&state);
    }
    for (pos = 0; pos < 256; ++pos)
        DR_REQUIRE(hits[pos] == (period ? 640 / period : 0));
    ok = 1;
end:
    return ok ? 0 : 1;
}

int
test_refresh_adaptive_boundary(int mode)
{
    sixel_delta_residual_t state;
    sixel_delta_mask_t original;
    sixel_encoder_t *encoder;
    int ok;

    ok = 0;
    encoder = NULL;
    memset(&state, 0, sizeof(state));
    sixel_delta_refresh_prepare(&state, 16, 16, 0, 0, 1, 0);
    refresh_commit(&state);
    sixel_delta_refresh_prepare(&state, 16, 16, 0, 0, 1, 1);
    if (mode == 0) {
        /* The legacy path is bit-identical with explicit zero modifiers. */
        original = state.mask;
        sixel_delta_refresh_adapt(&state, 0, 0, 1, 0, 0, NULL);
        DR_REQUIRE(memcmp(&original, &state.mask, sizeof(original)) == 0);
    } else if (mode == 1) {
        /* A floor cannot raise the runtime cap or the configured T. */
        sixel_delta_refresh_adapt(&state, 0, 0, 1, 8, 0, NULL);
        DR_REQUIRE(sixel_delta_mask_threshold(&state.mask, 0, 0, 4) == 4);
        DR_REQUIRE(sixel_delta_mask_threshold(&state.mask, 0, 0, 48) == 8);
    } else if (mode == 2) {
        /* Geometry recovery must reevaluate even errors below the floor. */
        refresh_commit(&state);
        sixel_delta_refresh_prepare(&state, 16, 16, 1, 0, 1, 1);
        sixel_delta_refresh_adapt(&state, 1, 0, 1, 8, 2, NULL);
        DR_REQUIRE(sixel_delta_mask_threshold(&state.mask, 0, 0, 48) == 0);
    } else {
        DR_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
            "none") == SIXEL_OK);
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T48:J5:L4:A1") == SIXEL_OK);
        encoder->residual_state.refresh_frame = 23;
        encoder->residual_state.refresh_valid = 1;
        if (mode == 3 || mode == 4) {
            DR_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_UPDATE_POLICY,
                mode == 3 ? "delta:L256" : "delta:A3") != SIXEL_OK);
            DR_REQUIRE(encoder->refresh_floor == 4);
            DR_REQUIRE(encoder->refresh_curve == 1);
            DR_REQUIRE(encoder->residual_state.refresh_frame == 23);
        } else if (mode == 5) {
            DR_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_UPDATE_POLICY, "delta:T48:J5:L8:A1") == SIXEL_OK);
            DR_REQUIRE(!encoder->residual_state.refresh_valid);
        } else if (mode == 6) {
            DR_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_UPDATE_POLICY, "delta:T48:J5:L4:A2") == SIXEL_OK);
            DR_REQUIRE(!encoder->residual_state.refresh_valid);
        } else if (mode == 7) {
            DR_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_UPDATE_POLICY, "delta:T48:J5:L4:A1") == SIXEL_OK);
            DR_REQUIRE(encoder->residual_state.refresh_frame == 23);
            DR_REQUIRE(encoder->residual_state.refresh_valid);
        } else {
            DR_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_DIFFUSION, "fs") == SIXEL_OK);
            DR_REQUIRE(encoder->method_for_diffuse == SIXEL_DIFFUSE_FS);
        }
    }
    ok = 1;
end:
    sixel_encoder_unref(encoder);
    return ok ? 0 : 1;
}

/* A changed uniform gray gives an independent wire-color oracle. All low
 * error positions must eventually reach RGB101 42 (RGB8 107); the floor
 * then keeps that color rather than repeatedly emitting the same sample. */
int
test_refresh_adaptive_pixels(char const *gpu)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    refresh_output_t out;
    unsigned char pixels[16 * 16 * 3];
    unsigned int tick, i, changed, previous;
    int mode, ok;

    mode = 0;
    ok = 0;
    previous = 0;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    memset(&out, 0, sizeof(out));
    DR_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
        "none") == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        gpu) == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta:T48:J1:L4:A1") == SIXEL_OK);
    DR_REQUIRE(sixel_output_new(&output, refresh_write, &out, NULL)
        == SIXEL_OK);
    DR_REQUIRE(sixel_frame_new(&frame, NULL) == SIXEL_OK);
    for (tick = 0; tick <= 16; ++tick) {
        memset(pixels, tick ? 108 : 102, sizeof(pixels));
        out.size = 0;
        DR_REQUIRE(sixel_frame_init_borrowed(frame, pixels, 16, 16,
            SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
        DR_REQUIRE(sixel_encoder_encode_frame(encoder, frame, output)
            == SIXEL_OK);
        changed = 0;
        for (i = 0; i < sizeof(pixels); i += 3) {
            DR_REQUIRE(encoder->accumulation_pixels[i] == 102 ||
                encoder->accumulation_pixels[i] == 107);
            DR_REQUIRE(encoder->accumulation_pixels[i] ==
                encoder->accumulation_pixels[i + 1]);
            DR_REQUIRE(encoder->accumulation_pixels[i] ==
                encoder->accumulation_pixels[i + 2]);
            changed += encoder->accumulation_pixels[i] == 107;
        }
        DR_REQUIRE(changed >= previous);
        if (!tick) DR_REQUIRE(changed == 0);
        if (tick == 1) DR_REQUIRE(changed > 0 && changed < 256);
        if (tick >= 8) DR_REQUIRE(changed == 256);
        previous = changed;
    }
    ok = 1;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? 0 : 1;
}

int
test_refresh_adaptive_gpu(char const *method)
{
    static refresh_output_t cpu[3], gpu[3];
    int frame_no;

    if (!refresh_encode_sequence(method, "off",
            "delta:T48:J1:L4:A1", cpu) ||
            !refresh_encode_sequence(method, "force",
                "delta:T48:J1:L4:A1", gpu)) return 1;
    for (frame_no = 0; frame_no < 3; ++frame_no) {
        if (cpu[frame_no].size != gpu[frame_no].size ||
                memcmp(cpu[frame_no].bytes, gpu[frame_no].bytes,
                    cpu[frame_no].size) != 0) return 1;
    }
    return 0;
}

#undef DR_REQUIRE
