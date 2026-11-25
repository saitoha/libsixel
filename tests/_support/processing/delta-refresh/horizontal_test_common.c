/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/delta-residual.h"
#include "src/bluenoise_64x64.h"

#define H8_REQUIRE(c) do { if (!(c)) { \
    fprintf(stderr, "horizontal8 line %d: %s (%s)\n", __LINE__, #c, \
        sixel_helper_get_additional_message()); return 1; } } while (0)

/* A full tile plus a clipped edge exercises wrapping and absolute origins.
 * Literal band periods are the oracle, independent of phase lookup code. */
int
test_refresh_horizontal_cadence(unsigned int period, unsigned int curve)
{
    sixel_delta_residual_t s;
    unsigned int last[4][65 * 65];
    unsigned int spans[4], errors[4], f, x, y, p, b, t, due, previous;
    unsigned int span, frames;
    int origin_x, origin_y;

    memset(&s, 0, sizeof(s));
    memset(last, 0, sizeof(last));
    s.refresh_mask = SIXEL_REFRESH_MASK_HORIZONTAL8;
    s.width = 65;
    s.height = 65;
    origin_x = 29;
    origin_y = 51;
    s.refresh_origin_x = origin_x;
    s.refresh_origin_y = origin_y;
    spans[0] = period * (curve ? 8U : 1U);
    spans[1] = period * (curve ? 4U : 1U);
    spans[2] = period * (curve ? 2U : 1U);
    spans[3] = period;
    errors[0] = 8;
    errors[1] = 16;
    errors[2] = 32;
    errors[3] = 33;
    frames = spans[0] * 2;
    for (f = 0; f <= frames; ++f) {
        sixel_delta_refresh_prepare(&s, 65, 65, origin_x, origin_y,
            period, f != 0);
        sixel_delta_refresh_adapt(&s, origin_x, origin_y, period, 0,
            curve, NULL);
        for (y = 0; y < 65; ++y) {
            for (x = 0; x < 65; ++x) {
                p = y * 65 + x;
                t = sixel_delta_mask_threshold(&s.mask, x, y, 255);
                if (x && ((x + origin_x + 37) & 7U))
                    H8_REQUIRE(t == sixel_delta_mask_threshold(&s.mask,
                        x - 1, y, 255));
                previous = 0;
                for (b = 0; b < 4; ++b) {
                    due = errors[b] > t;
                    H8_REQUIRE(!previous || due);
                    previous = due;
                    if (!due) continue;
                    span = spans[b];
                    H8_REQUIRE(f && (last[b][p] ?
                        f - last[b][p] == span : f <= span));
                    last[b][p] = f;
                }

            }
        }
        s.refresh_frame = s.refresh_next_frame;
        s.refresh_valid = s.refresh_next_valid;
    }
    for (p = 0; p < 65 * 65; ++p)
        for (b = 0; b < 4; ++b)
            H8_REQUIRE(last[b][p] && frames - last[b][p] < spans[b]);
    /* Moving a rectangle restarts with T0 against valid display history. */
    sixel_delta_refresh_prepare(&s, 65, 65, origin_x + 1, origin_y,
        period, 1);
    sixel_delta_refresh_adapt(&s, origin_x + 1, origin_y, period, 0,
        curve, NULL);
    H8_REQUIRE(s.refresh_next_frame == 0 && s.selected_pixels == 65 * 65);
    H8_REQUIRE(sixel_delta_mask_threshold(&s.mask, 64, 64, 255) == 0);
    return 0;
}

/* Reconstruct the original translated phase schedule independently. */
int
test_refresh_bluenoise_unchanged(unsigned int curve)
{
    sixel_delta_residual_t s;
    unsigned int f, x, y, phase, cycle, expected, due, band;
    unsigned int residues[4], multiples[4], cuts[4] = {0, 8, 16, 32};

    memset(&s, 0, sizeof(s));
    s.width = s.height = 64;
    for (f = 0; f <= 80; ++f) {
        sixel_delta_refresh_prepare(&s, 64, 64, 0, 0, 5, f != 0);
        sixel_delta_refresh_adapt(&s, 0, 0, 5, 0, curve, NULL);
        for (band = 0; band < 4; ++band) {
            multiples[band] = 1U << ((3U - band) * curve);
            residues[band] = f ? ((f - 1) / 5) % multiples[band] : 0;
        }
        for (y = 0; y < 64; ++y) {
            for (x = 0; x < 64; ++x) {
                phase = sixel_bn64[((y + 17) & 63) * 64 +
                    ((x + 37) & 63)] >> 2;
                due = f && ((phase + 1) * 5 + 63) / 64 ==
                    (f - 1) % 5 + 1;
                expected = 255;
                if (due) {
                    expected = 0;
                    if (curve) {
                        cycle = sixel_bn64[((y + 43) & 63) * 64 +
                            ((x + 11) & 63)] >> 2;
                        for (band = 0; band < 3; ++band)
                            if (cycle % multiples[band] == residues[band])
                                break;
                        expected = cuts[band];
                    }
                }
                H8_REQUIRE(sixel_delta_mask_threshold(&s.mask, x, y, 255)
                    == expected);
            }
        }
        s.refresh_frame = s.refresh_next_frame;
        s.refresh_valid = s.refresh_next_valid;
    }
    return 0;
}

static int
h8_discard(char *data, int size, void *context)
{
    (void)data;
    (void)context;
    return size;
}

int
test_refresh_horizontal_configuration(int mode)
{
    sixel_encoder_t *e;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    unsigned int periods[4] = {16, 8, 4, 2};
    sixel_delta_residual_t saved;
    int mask;
    SIXELSTATUS status;
    uint64_t revision;

    e = NULL;
    output = NULL;
    playback = NULL;
    H8_REQUIRE(sixel_encoder_new(&e, NULL) == SIXEL_OK);
    H8_REQUIRE(sixel_encoder_setopt(e, 'Z', "delta:T24:J2") == SIXEL_OK);
    if (mode == 0 || mode == 3 || mode == 4) {
        H8_REQUIRE(sixel_encoder_set_6delta_refresh_mask(e,
            SIXEL_REFRESH_MASK_HORIZONTAL8) == SIXEL_OK);
        e->residual_state.refresh_valid = 1;
        e->residual_state.refresh_frame = 7;
        saved = e->residual_state;
        revision = e->configuration_revision;
        if (mode == 0) {
            H8_REQUIRE(sixel_encoder_set_6delta_refresh_mask(e, 2)
                == SIXEL_BAD_ARGUMENT);
            H8_REQUIRE(sixel_encoder_setopt(e, 'Z',
                "delta:J2:refresh_mask=invalid") == SIXEL_BAD_ARGUMENT);
            H8_REQUIRE(e->refresh_mask == SIXEL_REFRESH_MASK_HORIZONTAL8);
            H8_REQUIRE(e->configuration_revision == revision);
            H8_REQUIRE(!memcmp(&saved, &e->residual_state, sizeof(saved)));
        } else if (mode == 3) {
            H8_REQUIRE(sixel_encoder_set_6delta_refresh_mask(e,
                SIXEL_REFRESH_MASK_HORIZONTAL8) == SIXEL_OK);
            H8_REQUIRE(e->configuration_revision == revision);
            H8_REQUIRE(!memcmp(&saved, &e->residual_state, sizeof(saved)));
        } else {
            H8_REQUIRE(sixel_encoder_setopt(e, 'Z', "delta:T24:J2")
                == SIXEL_OK);
            H8_REQUIRE(e->refresh_mask == SIXEL_REFRESH_MASK_BLUENOISE);
            H8_REQUIRE(!e->residual_state.refresh_valid);
            H8_REQUIRE(e->configuration_revision > revision);
        }
    } else if (mode == 1) {
        H8_REQUIRE(sixel_encoder_setopt(e, 'Z',
            "delta:refresh_mask=horizontal8") == SIXEL_OK);
        H8_REQUIRE(e->refresh_frames == 0 && e->sixdelta_threshold == 0);
    } else if (mode == 5) {
        H8_REQUIRE(sixel_encoder_setopt(e, 'Z',
            "delta:J2:A2:refresh_mask=horizontal8") == SIXEL_BAD_ARGUMENT);
        H8_REQUIRE(e->refresh_mask == SIXEL_REFRESH_MASK_BLUENOISE);
    } else if (mode == 6) {
        H8_REQUIRE(sixel_encoder_setopt(e, 'Z',
            "delta:J2:G1:refresh_mask=horizontal8") == SIXEL_BAD_ARGUMENT);
        H8_REQUIRE(e->refresh_mask == SIXEL_REFRESH_MASK_BLUENOISE);
    } else if (mode == 7) {
        H8_REQUIRE(sixel_encoder_setopt(e, 'Z',
            "delta:J0:refresh_mask=horizontal8") == SIXEL_OK);
        H8_REQUIRE(sixel_encoder_set_6delta_refresh_periods(e, periods)
            == SIXEL_BAD_ARGUMENT);
        H8_REQUIRE(e->refresh_periods[3] == 0);
    } else if (mode == 8) {
        H8_REQUIRE(sixel_encoder_setopt(e, 'G', "force") == SIXEL_OK);
        H8_REQUIRE(sixel_encoder_set_6delta_refresh_mask(e,
            SIXEL_REFRESH_MASK_HORIZONTAL8) == SIXEL_OK);
        H8_REQUIRE(e->refresh_mask == SIXEL_REFRESH_MASK_HORIZONTAL8);
    } else if (mode == 9) {
        H8_REQUIRE(sixel_encoder_setopt(e, 'G', "auto") == SIXEL_OK);
        H8_REQUIRE(sixel_encoder_set_6delta_refresh_mask(e,
            SIXEL_REFRESH_MASK_HORIZONTAL8) == SIXEL_OK);
        H8_REQUIRE(sixel_encoder_setopt(e, 'G', "force")
            == SIXEL_OK);
        H8_REQUIRE(e->gpu_policy == SIXEL_GPU_POLICY_FORCE);
    } else {
        H8_REQUIRE(sixel_playback_config_init(&config, sizeof(config))
            == SIXEL_OK);
        H8_REQUIRE(sixel_output_new(&output, h8_discard, NULL, NULL)
            == SIXEL_OK);
        H8_REQUIRE(sixel_playback_new(&playback, e, output, &config, NULL)
            == SIXEL_OK);
        mask = e->refresh_mask;
        status = sixel_encoder_set_6delta_refresh_mask(e,
            SIXEL_REFRESH_MASK_HORIZONTAL8);
        H8_REQUIRE(status != SIXEL_OK && e->refresh_mask == mask);
    }
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(e);
    return 0;
}

typedef struct h8_capture {
    unsigned char data[65536];
    size_t size;
    int fail;
} h8_capture_t;

static int
h8_write(char *bytes, int size, void *context)
{
    h8_capture_t *out;

    out = context;
    if (out->fail || size < 0 || (size_t)size > sizeof(out->data) - out->size)
        return -1;
    memcpy(out->data + out->size, bytes, (size_t)size);
    out->size += (size_t)size;
    return size;
}

/* Compare complete stateful sequences, not just an option's stored value.
 * The positional GPU case checks dispatch and CPU-equivalent output. */
int
test_refresh_horizontal_output(int mode)
{
    sixel_encoder_t *e[2];
    sixel_frame_t *frame;
    sixel_output_t *output;
    h8_capture_t out[2];
    size_t frame_start;
    uint32_t sample;
    unsigned char rgb[16 * 16 * 3];
    unsigned char retained[sizeof(rgb)];
    unsigned int n, i, p;
    char const *policy;
    uint64_t age;
    SIXELSTATUS status;

    memset(out, 0, sizeof(out));
    frame = NULL;
    output = NULL;
    for (i = 0; i < 2; ++i) {
        H8_REQUIRE(sixel_encoder_new(&e[i], NULL) == SIXEL_OK);
        H8_REQUIRE(sixel_encoder_setopt(e[i], 'p', "16") == SIXEL_OK);
        if (mode == 0) {
            /* Compare ordered application with the same host solver.
             * The GPU SSE splitter uses FP32 and can choose other cuts. */
            H8_REQUIRE(sixel_encoder_setopt(e[i], 'Q',
                "heckbert:cut=median") == SIXEL_OK);
        }
        H8_REQUIRE(sixel_encoder_setopt(e[i], 'd',
            mode == 5 ? "lso2" : mode == 3 ? "fs" : "bluenoise")
            == SIXEL_OK);
        H8_REQUIRE(sixel_encoder_setopt(e[i], '~',
            mode == 0 || mode == 3 || mode == 5 ? "ordered" : "none")
            == SIXEL_OK);
        H8_REQUIRE(sixel_encoder_setopt(e[i], 'G',
            (mode == 3 || mode == 5)
                ? (i ? "auto:palette_threshold=0" : "force")
                : mode == 0 && i ? "force" : "off") == SIXEL_OK);
        policy = mode == 1 ? (i ?
            "delta:T24:J2:A1:refresh_mask=bluenoise" : "delta:T24:J2:A1")
            : "delta:T24:J2:A1:refresh_mask=horizontal8";
        H8_REQUIRE(sixel_encoder_setopt(e[i], 'Z', policy) == SIXEL_OK);
        if (mode == 3 || mode == 5)
            H8_REQUIRE(sixel_encoder_diffuse_on_gpu(e[i]));
    }
    for (n = 0; n < 5; ++n) {
        frame_start = out[0].size;
        for (p = 0; p < sizeof(rgb); ++p) {
            sample = p;
            if (mode == 0) {
                /* This fixed distribution separates CPU and GPU SSE
                 * palettes on the first frame without the host cut. */
                sample = (p + 1234u) ^ 1u;
                sample ^= sample >> 16;
                sample *= 0x7feb352du;
                sample ^= sample >> 15;
                sample *= 0x846ca68bu;
                sample ^= sample >> 16;
            }
            rgb[p] = (unsigned char)(60 + sample % 80 + n * 2);
        }
        for (i = 0; i < 2; ++i) {
            H8_REQUIRE(sixel_frame_new(&frame, NULL) == SIXEL_OK);
            H8_REQUIRE(sixel_frame_init_borrowed(frame, rgb, 16, 16,
                SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
            sixel_frame_set_multiframe(frame, 1);
            H8_REQUIRE(sixel_output_new(&output, h8_write, &out[i], NULL)
                == SIXEL_OK);
            H8_REQUIRE(sixel_encoder_encode_frame(e[i], frame, output)
                == SIXEL_OK);
            sixel_output_unref(output);
            sixel_frame_unref(frame);
        }
        if (mode == 0) {
            H8_REQUIRE(out[0].size == out[1].size);
            H8_REQUIRE(!memcmp(out[0].data + frame_start,
                out[1].data + frame_start, out[0].size - frame_start));
            H8_REQUIRE(!memcmp(e[0]->accumulation_pixels,
                e[1]->accumulation_pixels, sizeof(rgb)));
        }
    }
    H8_REQUIRE(out[0].size && out[0].size == out[1].size);
    if (mode != 0) {
        H8_REQUIRE(!memcmp(out[0].data, out[1].data, out[0].size));
        H8_REQUIRE(!memcmp(e[0]->accumulation_pixels,
            e[1]->accumulation_pixels, sizeof(rgb)));
    }
    if (mode == 2) {
        age = e[0]->residual_state.refresh_frame;
        memcpy(retained, e[0]->accumulation_pixels, sizeof(retained));
        H8_REQUIRE(sixel_encoder_set_6delta_refresh_mask(e[0],
            SIXEL_REFRESH_MASK_HORIZONTAL8) == SIXEL_OK);
        H8_REQUIRE(age == e[0]->residual_state.refresh_frame);
        H8_REQUIRE(sixel_encoder_set_6delta_refresh_mask(e[0],
            SIXEL_REFRESH_MASK_BLUENOISE) == SIXEL_OK);
        H8_REQUIRE(e[0]->accumulation_valid &&
            !memcmp(retained, e[0]->accumulation_pixels, sizeof(retained)));
        H8_REQUIRE(!e[0]->residual_state.refresh_valid);
    }
    if (mode == 4) {
        out[0].fail = 1;
        H8_REQUIRE(sixel_frame_new(&frame, NULL) == SIXEL_OK);
        H8_REQUIRE(sixel_frame_init_borrowed(frame, rgb, 16, 16,
            SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
        H8_REQUIRE(sixel_output_new(&output, h8_write, &out[0], NULL)
            == SIXEL_OK);
        status = sixel_encoder_encode_frame(e[0], frame, output);
        H8_REQUIRE(status != SIXEL_OK);
        H8_REQUIRE(!e[0]->accumulation_valid &&
            !e[0]->residual_state.refresh_valid);
        sixel_output_unref(output);
        sixel_frame_unref(frame);
    }
    sixel_encoder_unref(e[0]);
    sixel_encoder_unref(e[1]);
    return 0;
}

/* The host tile is the oracle for transport/application. Exact gray output
 * makes paint/keep decisions observable without a perceptual tolerance.
 * The separate cadence cases establish that this tile meets deadlines. */
int
test_refresh_horizontal_gpu_mask(unsigned int curve)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    h8_capture_t out;
    unsigned char rgb[64 * 64 * 3], expected[64 * 64];
    unsigned int f, x, y, p, period, frames, threshold;

    memset(&out, 0, sizeof(out));
    memset(expected, 102, sizeof(expected));
    encoder = NULL;
    output = NULL;
    frame = NULL;
    period = curve ? 2 : 5;
    frames = curve ? 16 : 5;
    H8_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    H8_REQUIRE(sixel_encoder_setopt(encoder, 'd', curve
        ? "bluenoise:strength=0" : "none") == SIXEL_OK);
    H8_REQUIRE(sixel_encoder_setopt(encoder, '~',
        curve ? "ordered" : "none") == SIXEL_OK);
    H8_REQUIRE(sixel_encoder_setopt(encoder, 'G', "force") == SIXEL_OK);
    H8_REQUIRE(sixel_encoder_setopt(encoder, 'Z', curve
        ? "delta:T48:J2:A1:refresh_mask=horizontal8"
        : "delta:T48:J5:A0:refresh_mask=horizontal8") == SIXEL_OK);
    H8_REQUIRE(sixel_output_new(&output, h8_write, &out, NULL) == SIXEL_OK);
    for (f = 0; f <= frames; ++f) {
        memset(rgb, f ? 108 : 102, sizeof(rgb));
        H8_REQUIRE(sixel_frame_new(&frame, NULL) == SIXEL_OK);
        H8_REQUIRE(sixel_frame_init_borrowed(frame, rgb, 64, 64,
            SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
        out.size = 0;
        H8_REQUIRE(sixel_encoder_encode_frame(encoder, frame, output)
            == SIXEL_OK);
        H8_REQUIRE(encoder->residual_state.refresh_frame == f);
        for (y = 0; y < 64; ++y) {
            for (x = 0; x < 64; ++x) {
                p = y * 64 + x;
                threshold = sixel_delta_mask_threshold(
                    &encoder->residual_state.mask, x, y, 48);
                if (f && threshold < 6) expected[p] = 107;
                H8_REQUIRE(encoder->accumulation_pixels[p * 3] ==
                    expected[p]);
                H8_REQUIRE(encoder->accumulation_pixels[p * 3 + 1] ==
                    expected[p]);
                H8_REQUIRE(encoder->accumulation_pixels[p * 3 + 2] ==
                    expected[p]);
                if (f == frames) H8_REQUIRE(expected[p] == 107);
            }
        }
        sixel_frame_unref(frame);
    }
    H8_REQUIRE(period == 2 || period == 5);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return 0;
}
