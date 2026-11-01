/* SPDX-License-Identifier: MIT */
/* A stride-one grid retains solid-region mass above its pixel population. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/filter-factory.h"
#include "src/filter-sample.h"

int
test_palette_0079_sampling_small_solid(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_filter_t *filter;
    sixel_filter_sample_config_t config;
    sixel_sample_stream_t samples;
    unsigned char pixels[32 * 32 * 3];
    unsigned char const *sample_pixels;
    size_t index;
    int attempt;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    frame = NULL;
    filter = NULL;
    ok = 0;
    memset(pixels, 128, sizeof(pixels));
    memset(&config, 0, sizeof(config));
    sixel_sample_stream_init(&samples);
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 32, 32,
                SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
        goto end;
    }
    /* Default, target equal to input, and target above input all use stride
     * one. Full-frame borrows exactly 1024 pixels; grid sampling appends
     * solid-color weights and padding. The multiset is therefore different. */
    for (attempt = 0; attempt < 4; ++attempt) {
        config.policy = attempt == 3 ? SIXEL_PALETTE_SAMPLING_FULL_FRAME
            : SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID;
        config.source = attempt == 3
            ? SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME
            : SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME;
        config.reqcolors = 256;
        config.palette_sample_override = attempt != 0;
        config.palette_sample_target = attempt == 1 ? 1024u : 2048u;
        if (SIXEL_FAILED(sixel_filter_factory_create_by_kind(
                    SIXEL_FILTER_KIND_SAMPLE, &config, &filter))) {
            goto end;
        }
        sixel_filter_bind_input(filter, &frame, SIXEL_PIXELFORMAT_RGB888,
            sixel_frame_get_colorspace(frame));
        sixel_filter_bind_sample_output(filter, &samples,
            SIXEL_PIXELFORMAT_RGB888, sixel_frame_get_colorspace(frame));
        if (SIXEL_FAILED(sixel_filter_run(filter, allocator, NULL)) ||
                samples.width != 32 ||
                (attempt == 3 && samples.point_count != 1024u) ||
                (attempt != 3 && samples.point_count <= 1024u)) {
            goto end;
        }
        sample_pixels = sixel_frame_get_pixels(samples.frame);
        for (index = 0u; index < samples.point_count * 3u; ++index) {
            if (sample_pixels[index] != 128) {
                goto end;
            }
        }
        sixel_filter_free(filter);
        filter = NULL;
        sixel_sample_stream_dispose(&samples);
    }
    ok = 1;
end:
    sixel_filter_free(filter);
    sixel_sample_stream_dispose(&samples);
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    if (!ok) {
        fprintf(stderr, "small solid sample population contract failed\n");
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
