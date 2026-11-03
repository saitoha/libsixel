/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/bluenoise_64x64.h"

/* Every wrapper chooses one rectangle/time transition and checks the entire
 * retained canvas against independent wire RGB values, including untouched
 * pixels outside the damage. No perceptual score can hide a misplaced keep. */
#define REQUIRE(x) do { if (!(x)) { \
    fprintf(stderr, "partial refresh %d line %d: %s (%s)\n", \
        mode, __LINE__, #x, sixel_helper_get_additional_message()); \
    goto end; } } while (0)

typedef struct partial_output {
    size_t size;
    int fail;
} partial_output_t;

static int
partial_write(char *data, int size, void *opaque)
{
    partial_output_t *out;

    (void)data;
    out = opaque;
    if (size < 0 || out->fail) return -1;
    out->size += (size_t)size;
    return size;
}

int
test_partial_refresh(int mode, int gpu)
{
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    partial_output_t out;
    unsigned char pixels[96 * 72 * 3];
    sixel_frame_interface_t *frame_if;
    sixel_frame_transparency_t transparency;
    unsigned char *mask;
    unsigned int rank;
    size_t i, pos;
    int step, x, y, ox, oy, width, height, selected, expected, ok;
    SIXELSTATUS status;

    allocator = NULL;
    mask = NULL;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    playback = NULL;
    memset(&out, 0, sizeof(out));
    ok = 0;
    REQUIRE(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)
        == SIXEL_OK);
    REQUIRE(sixel_encoder_new(&encoder, allocator) == SIXEL_OK);
    REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
        "none") == SIXEL_OK);
    REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_LUT_POLICY,
        "ordered") == SIXEL_OK);
    REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        gpu ? "force" : "off") == SIXEL_OK);
    REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta:T8:J64") == SIXEL_OK);
    REQUIRE(sixel_encoder_set_6delta_plane_size(encoder, 96, 72) == SIXEL_OK);
    REQUIRE(sixel_output_new(&output, partial_write, &out, NULL) == SIXEL_OK);
    REQUIRE(sixel_frame_new(&frame, allocator) == SIXEL_OK);
    REQUIRE(sixel_playback_options_init(&options, sizeof(options)) == SIXEL_OK);
    options.flags = SIXEL_PLAYBACK_OPTION_ORIGIN |
        SIXEL_PLAYBACK_OPTION_REFRESH_TIME;
    if (mode == 2 || mode == 3) {
        REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADING_POLICY,
            "balanced:global_budget=2:prepare=ahead") == SIXEL_OK);
        REQUIRE(sixel_playback_config_init(&config, sizeof(config))
            == SIXEL_OK);
        config.palette_mode = mode == 3 ? SIXEL_PLAYBACK_PALETTE_LATEST
            : SIXEL_PLAYBACK_PALETTE_PER_FRAME;
        REQUIRE(sixel_playback_new(&playback, encoder, output, &config, NULL)
            == SIXEL_OK);
    }
    ox = 7;
    oy = 11;
    width = 32;
    height = 24;
    for (step = 0; step < (mode == 13 ? 4 : 3); ++step) {
        memset(pixels, step < 2 || (mode == 13 && step == 2)
            ? 102 : 108, sizeof(pixels));
        if (step == 2) {
            if (mode == 4) ox = 9;
            if (mode == 5) width = 31;
            if (mode == 6) oy = 13;
            if (mode == 10) ox = 80;
            if (mode == 12) ox = 9;
            if (mode == 13) ox = 50;
            if (mode == 14) height = 23;
        }
        if (step == 3) ox = 7;
        REQUIRE(sixel_frame_init_borrowed(frame, pixels,
            step ? width : 96, step ? height : 72,
            SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
        options.origin_x = step ? ox : 0;
        options.origin_y = step ? oy : 0;
        options.refresh_time_us = step < 2 ? 0U :
            (mode == 7 || mode == 12) ? 0U :
            mode == 8 ? 80000U : step == 3 ? 32000U : 16000U;
        if (step == 2 && mode == 9) out.fail = 1;
        if (step == 2 && mode == 11) {
            mask = sixel_allocator_malloc(allocator, 32U * 24U);
            REQUIRE(mask != NULL);
            memset(mask, 0, 32U * 24U);
            mask[0] = 1;
            memset(&transparency, 0, sizeof(transparency));
            transparency.transparent = -1;
            transparency.transparent_mask = mask;
            transparency.transparent_mask_size = 32U * 24U;
            frame_if = sixel_frame_as_interface(frame);
            REQUIRE(frame_if->vtbl->set_transparency(frame_if, &transparency)
                == SIXEL_OK);
            mask = NULL;
        }
        out.size = 0;
        if (playback != NULL) {
            status = sixel_playback_submit(playback, frame,
                SIXEL_PLAYBACK_WAIT, -1, &options);
            if (status == SIXEL_OK)
                status = sixel_playback_drain(playback, -1);
        } else {
            REQUIRE(sixel_encoder_set_6delta_plane_origin(encoder,
                options.origin_x, options.origin_y) == SIXEL_OK);
            encoder->refresh_time_override = 1;
            encoder->refresh_time_us = options.refresh_time_us;
            status = sixel_encoder_encode_frame(encoder, frame, output);
        }
        if (step == 2 && mode >= 9 && mode <= 11) {
            REQUIRE(SIXEL_FAILED(status));
            if (mode == 9) REQUIRE(!encoder->residual_state.refresh_valid);
            else REQUIRE(out.size == 0);
            ok = 1;
            goto end;
        }
        REQUIRE(status == SIXEL_OK);
    }
    for (y = 0; y < 72; ++y) {
        for (x = 0; x < 96; ++x) {
            selected = x >= ox && x < ox + width &&
                y >= oy && y < oy + height;
            rank = sixel_bn64[((y + 17) & 63) * 64 + ((x + 37) & 63)];
            if (mode == 7) selected = 0;
            else if (mode <= 3) selected = selected && rank < 64U;
            if (mode == 3 && x >= ox && x < ox + width &&
                    y >= oy && y < oy + height) {
                /* Latest may still carry the earlier all-102 palette. The
                 * contract here is deadline selection, not guaranteed paint
                 * when the available palette has no improving candidate. */
                REQUIRE(sixel_delta_mask_threshold(
                    &encoder->residual_state.mask, x - ox, y - oy, 8) ==
                    (rank < 64U ? 0U : 8U));
            }
            expected = selected ? 107 : 102;
            pos = ((size_t)y * 96U + (size_t)x) * 3U;
            for (i = 0; i < 3; ++i) {
                if (mode == 3 && selected &&
                        encoder->accumulation_pixels[pos + i] == 102)
                    continue;
                if (encoder->accumulation_pixels[pos + i] != expected) {
                    fprintf(stderr, "rect=%d,%d selected=%lu last=%llu\n",
                        encoder->residual_state.refresh_origin_x,
                        encoder->residual_state.refresh_origin_y,
                        (unsigned long)encoder->residual_state.selected_pixels,
                        (unsigned long long)
                            encoder->residual_state.refresh_last_us);
                    fprintf(stderr, "pixel (%d,%d): %u != %d\n", x, y,
                        encoder->accumulation_pixels[pos + i], expected);
                    goto end;
                }
            }
        }
    }
    ok = 1;
end:
    if (allocator != NULL) sixel_allocator_free(allocator, mask);
    sixel_playback_unref(playback);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}
