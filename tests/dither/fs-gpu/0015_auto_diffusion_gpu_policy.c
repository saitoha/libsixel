/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/rendezvous.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "src/dither.h"
#include "src/sixel_decode_pixels.h"

#if SIXEL_ENABLE_THREADS
enum { MASK_WIDTH = 64, MASK_HEIGHT = 48, MASK_FRAMES = 3 };

typedef struct mask_playback {
    playback_test_buffer_t wire;
    sixel_playback_t *playback;
    size_t ends[MASK_FRAMES];
    unsigned int stages[MASK_FRAMES];
    int count;
    int failed;
} mask_playback_t;

static void
mask_playback_done(sixel_playback_result_t const *result, void *context)
{
    mask_playback_t *capture;
    sixel_playback_stage_sample_t sample = {0};

    capture = context;
    sample.struct_size = sizeof(sample);
    if (capture->count == MASK_FRAMES || result->status != SIXEL_OK ||
            sixel_playback_get_stage_sample(capture->playback,
                result->sequence, &sample) != SIXEL_OK) {
        capture->failed = 1;
        return;
    }
    capture->ends[capture->count] = capture->wire.size;
    capture->stages[capture->count++] = sample.valid;
}

/* Alternate between disjoint strips of a static image. The third frame
 * revisits the first strip, so T0 should retain both strips without painting.
 * Color ramps force diffusion while keeping the generated palettes stable. */
static int
mask_playback_visible(int x, int frame)
{
    return x / 16 == frame % 2;
}

static int
test_keep_playback(char const *alpha, int delta)
{
    enum { PIXELS = MASK_WIDTH * MASK_HEIGHT };
    unsigned char source[PIXELS * 3], retained[PIXELS * 3], valid[PIXELS];
    unsigned char *pixels = NULL, *palette = NULL, *paint = NULL;
    sixel_frame_transparency_t coverage = {0};
    sixel_frame_interface_t *iface;
    sixel_encoder_t *encoder = NULL;
    sixel_output_t *output = NULL;
    sixel_frame_t *frame = NULL;
    sixel_playback_config_t config;
    mask_playback_t capture = {0};
    sixel_gpu_statistics_t before, after;
    size_t start, size, masked_bytes = 0;
    int i, x, y, base, variation, order, opaque, device, w, h, colors, visible;
    int ok = 0;
    unsigned int flags;

    device = lso2_gpu_rendezvous_device();
    AHEAD_CHECK(device == 0 || device == 77);
    for (i = 0; i < PIXELS; ++i) {
        x = i % MASK_WIDTH;
        y = i / MASK_WIDTH;
        base = ((x / 8 + y / 6) % 3) * 80 + 24;
        variation = ((x + y) % 4) * 4;
        source[3 * i] = (unsigned char)(base + variation);
        source[3 * i + 1] = (unsigned char)(base + variation + 5);
        source[3 * i + 2] = (unsigned char)(base + variation + 10);
    }
    for (opaque = 0; opaque < 2; ++opaque) {
        capture.count = 0;
        capture.wire.size = 0;
        AHEAD_CHECK(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
        AHEAD_CHECK(sixel_encoder_setopt(encoder, 'A', alpha) == SIXEL_OK);
        AHEAD_CHECK(sixel_encoder_setopt(encoder, 'G', "auto") == SIXEL_OK);
        AHEAD_CHECK(sixel_encoder_setopt(encoder, 'd', "fs") == SIXEL_OK);
        AHEAD_CHECK(sixel_encoder_setopt(encoder, '~', "ordered") == SIXEL_OK);
        AHEAD_CHECK(sixel_encoder_setopt(encoder, 'p', "4") == SIXEL_OK);
        AHEAD_CHECK(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_THREADING_POLICY,
            "greedy:B2:prepare=ahead:prepare_workers=1") == SIXEL_OK);
        AHEAD_CHECK(!delta || sixel_encoder_setopt(encoder, 'Z', "delta:T0")
            == SIXEL_OK);
        AHEAD_CHECK(sixel_output_new(&output, playback_test_write,
            &capture.wire, NULL) == SIXEL_OK);
        AHEAD_CHECK(sixel_playback_config_init(&config, sizeof(config))
            == SIXEL_OK);
        config.capacity = 4;
        config.done = mask_playback_done;
        config.done_context = &capture;
        AHEAD_CHECK(sixel_playback_new(&capture.playback, encoder, output,
            &config, NULL) == SIXEL_OK);
        AHEAD_CHECK(sixel_gpu_get_statistics(&before) == SIXEL_OK);
        for (order = 0; order < MASK_FRAMES; ++order) {
            AHEAD_CHECK(sixel_frame_new(&frame, NULL) == SIXEL_OK);
            AHEAD_CHECK(sixel_frame_init_borrowed(frame, source,
                MASK_WIDTH, MASK_HEIGHT, SIXEL_PIXELFORMAT_RGB888, NULL, -1)
                == SIXEL_OK);
            if (!opaque) {
                coverage.transparent = -1;
                coverage.alpha_zero_is_transparent = 1;
                coverage.transparent_mask_size = PIXELS;
                coverage.transparent_mask = malloc(PIXELS);
                AHEAD_CHECK(coverage.transparent_mask != NULL);
                for (i = 0; i < PIXELS; ++i) {
                    coverage.transparent_mask[i] =
                        !mask_playback_visible(i % MASK_WIDTH, order);
                }
                iface = sixel_frame_as_interface(frame);
                AHEAD_CHECK(iface->vtbl->set_transparency(iface, &coverage)
                    == SIXEL_OK);
                coverage.transparent_mask = NULL;
            }
            AHEAD_CHECK(sixel_playback_submit(capture.playback, frame,
                SIXEL_PLAYBACK_WAIT, -1, NULL) == SIXEL_OK);
            sixel_frame_unref(frame);
            frame = NULL;
        }
        AHEAD_CHECK(sixel_playback_drain(capture.playback, -1) == SIXEL_OK);
        AHEAD_CHECK(sixel_gpu_get_statistics(&after) == SIXEL_OK);
        AHEAD_CHECK(!capture.failed && capture.count == MASK_FRAMES);
        if (device == 0) {
            AHEAD_CHECK(after.dispatches > before.dispatches);
            for (order = 0; order < MASK_FRAMES; ++order) {
                AHEAD_CHECK(capture.stages[order] &
                    SIXEL_PLAYBACK_STAGE_DEFERRED);
            }
        }
        start = 0;
        memset(valid, 0, sizeof(valid));
        for (order = 0; !opaque && order < MASK_FRAMES; ++order) {
            size = capture.ends[order] - start;
            AHEAD_CHECK(size > 6 && size <= INT_MAX);
            AHEAD_CHECK(memcmp(capture.wire.bytes + start, "\033P0;1q", 6)
                == 0);
            AHEAD_CHECK(sixel_decode_raw_with_options_mask(
                capture.wire.bytes + start, (int)size, 0, &pixels, &paint,
                &w, &h, &palette, &colors, &flags, NULL) == SIXEL_OK);
            AHEAD_CHECK(w == MASK_WIDTH && h == MASK_HEIGHT && paint != NULL);
            for (i = 0; i < PIXELS; ++i) {
                visible = mask_playback_visible(i % MASK_WIDTH, order);
                AHEAD_CHECK((paint[i] != 0) ==
                    (visible && (!delta || order != 2)));
                if (paint[i]) {
                    AHEAD_CHECK(pixels[i] < colors);
                    memcpy(retained + 3 * i, palette + 3 * pixels[i], 3);
                    valid[i] = 1;
                }
            }
            free(pixels);
            free(paint);
            free(palette);
            pixels = paint = palette = NULL;
            start = capture.ends[order];
        }
        if (!opaque) {
            masked_bytes = capture.wire.size;
            if (delta) {
                AHEAD_CHECK(encoder->accumulation_valid_mask_size >= PIXELS);
                AHEAD_CHECK(encoder->accumulation_pixels_size >= 3 * PIXELS);
                for (i = 0; i < PIXELS; ++i) {
                    AHEAD_CHECK(valid[i] == (i % MASK_WIDTH < 32));
                    AHEAD_CHECK((encoder->accumulation_valid_mask[i] != 0)
                        == valid[i]);
                    if (valid[i]) {
                        AHEAD_CHECK(memcmp(encoder->accumulation_pixels + 3 * i,
                            retained + 3 * i, 3) == 0);
                    }
                }
            }
        }
        sixel_playback_unref(capture.playback);
        capture.playback = NULL;
        sixel_output_unref(output);
        output = NULL;
        sixel_encoder_unref(encoder);
        encoder = NULL;
    }
    /* Allow palette and run-length changes while rejecting an opaque packet. */
    AHEAD_CHECK(masked_bytes * 5 < capture.wire.size * 4);
    ok = 1;
end:
    free(coverage.transparent_mask);
    free(pixels);
    free(paint);
    free(palette);
    sixel_playback_unref(capture.playback);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    free(capture.wire.bytes);
    return !ok;
}
#endif

static SIXELSTATUS
encode_masked_playback(sixel_encoder_t *encoder, sixel_output_t *output,
                       sixel_frame_t *frame)
{
    sixel_playback_t *playback = NULL;
    sixel_playback_config_t config;
    SIXELSTATUS status;

    status = sixel_playback_config_init(&config, sizeof(config));
    if (status == SIXEL_OK) {
        config.capacity = 4;
        status = sixel_playback_new(&playback, encoder, output, &config, NULL);
    }
    if (status == SIXEL_OK) {
        status = sixel_playback_submit(playback, frame, SIXEL_PLAYBACK_WAIT,
                                       -1, NULL);
    }
    if (status == SIXEL_OK) {
        status = sixel_playback_drain(playback, -1);
    }
    sixel_playback_unref(playback);
    return status;
}

/* Four registers force diffusion on this 32-color input. Compare painted
 * colors so a valid change of register assignment still passes. */
static int
test_output_fallback(char const *mode)
{
    unsigned char source[32 * 3], reference[sizeof(source)];
    unsigned char *pixels = NULL, *palette = NULL, *paint = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t coverage = {0};
    sixel_encoder_t *encoder = NULL;
    sixel_frame_t *frame = NULL;
    sixel_output_t *output = NULL;
    playback_test_buffer_t buffer = {0};
    int i, pass, w, h, colors, masked, queued, ok = 0;
    unsigned int flags;

    masked = strncmp(mode, "mask-", 5) == 0;
    queued = strcmp(mode, "mask-playback") == 0;

    for (i = 0; i < (int)sizeof(source); ++i) {
        /* The three separated ramps keep the generated palette stable
         * across CPU and GPU-assisted construction for the masked case. */
        source[i] = masked
            ? (unsigned char)((i / 3 % 3) * 80 + 24 + i % 3 * 5 +
                              (i / 9 % 4) * 4)
            : (unsigned char)(i * 37);
    }
    AHEAD_CHECK(sixel_frame_new(&frame, NULL) == SIXEL_OK);
    AHEAD_CHECK(sixel_frame_init_borrowed(frame, source, 32, 1,
        SIXEL_PIXELFORMAT_RGB888, NULL, -1) == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'p', "4") == SIXEL_OK);
    if (masked) {
        coverage.transparent = -1;
        coverage.alpha_zero_is_transparent = 1;
        coverage.transparent_mask = calloc(32u, 1u);
        AHEAD_CHECK(coverage.transparent_mask != NULL);
        coverage.transparent_mask_size = 32u;
        coverage.transparent_mask[0] = 1;
        iface = sixel_frame_as_interface(frame);
        AHEAD_CHECK(iface->vtbl->set_transparency(iface, &coverage)
            == SIXEL_OK);
        coverage.transparent_mask = NULL;
        if (queued || strcmp(mode, "mask-clear") == 0) {
            AHEAD_CHECK(sixel_encoder_setopt(encoder, 'A', "clear")
                == SIXEL_OK);
        }
    } else {
        AHEAD_CHECK(sixel_encoder_setopt(encoder,
            strcmp(mode, "or") == 0 ? 'O' : 'U', mode) == SIXEL_OK);
    }
    if (queued) {
        AHEAD_CHECK(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_THREADING_POLICY,
            "greedy:B2:prepare=ahead:prepare_workers=1") == SIXEL_OK);
        AHEAD_CHECK(sixel_encoder_setopt(encoder, 'd', "atkinson")
            == SIXEL_OK);
    }
    AHEAD_CHECK(sixel_output_new(&output, playback_test_write, &buffer, NULL)
        == SIXEL_OK);

    for (pass = 0; pass < 2; ++pass) {
        buffer.size = 0;
        AHEAD_CHECK(sixel_encoder_setopt(encoder, 'G', pass ? "auto" : "off")
            == SIXEL_OK);
        AHEAD_CHECK((queued ? encode_masked_playback(encoder, output, frame)
            : sixel_encoder_encode_frame(encoder, frame, output))
                == SIXEL_OK);
        if (masked) {
            AHEAD_CHECK(buffer.size >= 6u &&
                memcmp(buffer.bytes, "\033P0;0q", 6u) == 0);
        }
        AHEAD_CHECK(sixel_decode_raw_with_options_mask(buffer.bytes,
            (int)buffer.size, 0, &pixels, &paint, &w, &h, &palette, &colors,
            &flags, NULL) == SIXEL_OK);
        AHEAD_CHECK(w == 32 && h == 1);
        for (i = 0; i < w; ++i) {
            AHEAD_CHECK(paint != NULL &&
                (paint[i] != 0) == !(masked && i == 0));
            if (!paint[i]) {
                memset(reference + 3 * i, 0, 3);
                continue;
            }
            AHEAD_CHECK(pixels[i] < colors);
            if (pass == 0) {
                memcpy(reference + 3 * i, palette + 3 * pixels[i], 3);
            } else {
                AHEAD_CHECK(memcmp(reference + 3 * i,
                    palette + 3 * pixels[i], 3) == 0);
            }
        }
        /* A solid reference could hide a failure to paint this gradient. */
        AHEAD_CHECK(memcmp(reference, reference + 3,
            sizeof(reference) - 3) != 0);
        free(paint);
        paint = NULL;
        free(pixels);
        pixels = NULL;
        free(palette);
        palette = NULL;
    }
    ok = 1;
end:
    free(coverage.transparent_mask);
    free(paint);
    free(pixels);
    free(palette);
    free(buffer.bytes);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_frame_unref(frame);
    return !ok;
}

int
test_dither_fs_gpu_0015(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    static char const *const policies[] = { "off", "auto", "force" };
    static char const *const colors[] = { "16", "64" };
    static int const methods[] = { SIXEL_DIFFUSE_ATKINSON, SIXEL_DIFFUSE_FS };
    playback_test_buffer_t buffer = { 0 };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t const *dither;
    sixel_gpu_statistics_t before, after;
    int ok, device;
    size_t i, j;

    if (argc == 3) {
        return test_keep_playback(argv[1], strcmp(argv[2], "delta") == 0);
    }
    if (argc == 2) return test_output_fallback(argv[1]);
    device = lso2_gpu_rendezvous_device();
    if (device != 0) return device;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    AHEAD_CHECK(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, '.', "8bit") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'W', "srgb") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'd', "auto") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, '~', "auto") == SIXEL_OK);
    AHEAD_CHECK(sixel_output_new(&output, playback_test_write, &buffer, NULL)
        == SIXEL_OK);
    frame = playback_test_small_frame(0);
    AHEAD_CHECK(frame != NULL);
    for (i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
        AHEAD_CHECK(sixel_encoder_setopt(encoder, 'p', colors[i]) == SIXEL_OK);
        for (j = 0; j < sizeof(policies) / sizeof(policies[0]); ++j) {
            AHEAD_CHECK(sixel_encoder_setopt(encoder, 'G', policies[j])
                == SIXEL_OK);
            AHEAD_CHECK(sixel_gpu_get_statistics(&before) == SIXEL_OK);
            AHEAD_CHECK(sixel_encoder_encode_frame(encoder, frame, output)
                == SIXEL_OK);
            AHEAD_CHECK(sixel_gpu_get_statistics(&after) == SIXEL_OK);
            dither = encoder->diagnostic_dither;
            AHEAD_CHECK(dither != NULL && dither->apply_success_count > 0);
            AHEAD_CHECK(dither->method_for_diffuse == methods[i]);
            AHEAD_CHECK(dither->diffuse_on_gpu == (j != 0));
            AHEAD_CHECK((after.dispatches > before.dispatches) == (j != 0));
        }
    }
    /* The GPU FHEDT preparer declines its CPU lookup cache option. */
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'G', "auto") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, '~',
        "fhedt:resolution=64:cache=1") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_encode_frame(encoder, frame, output) == SIXEL_OK);
    dither = encoder->diagnostic_dither;
    AHEAD_CHECK(dither != NULL && dither->apply_success_count > 0);
    AHEAD_CHECK(dither->method_for_diffuse == SIXEL_DIFFUSE_FS &&
        !dither->diffuse_on_gpu);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'G', "force") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_encode_frame(encoder, frame, output)
        == SIXEL_FEATURE_ERROR);
    ok = 1;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    free(buffer.bytes);
    return !ok;
#else
    if (argc == 2 && strcmp(argv[1], "mask-playback") != 0)
        return test_output_fallback(argv[1]);
    return 77;
#endif
}
