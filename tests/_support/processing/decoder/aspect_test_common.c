/* SPDX-License-Identifier: MIT */
/* Check literal samples through indexed+coverage, direct, packed and body
 * APIs. No encoder or second decode supplies the expected pixels. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/sixel_decode_pixels.h"
#include "aspect_test_common.h"

int
test_decoder_aspect_pixels(char const *stream, int width, int height,
    char const *expected, unsigned int options, unsigned int expected_flags)
{
    sixel_decode_options_t opts;
    sixel_decode_result_t result;
    SIXELSTATUS status;
    unsigned char *pixels;
    unsigned char *mask;
    unsigned char *palette;
    unsigned char rgb[4];
    unsigned char const *sample;
    unsigned int flags;
    unsigned int flag_mask;
    int w;
    int h;
    int nc;
    int mode;
    int pos;
    int ok;
    int params[3];
    char const *body;

    ok = 1;
    flag_mask = SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER |
        SIXEL_DECODE_PIXELS_RESULT_CLIPPED_TO_RASTER;
    memset(&opts, 0, sizeof(opts));
    opts.flags = options;
    opts.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    body = strchr(stream, 'q') + 1;
    params[0] = 0;
    params[1] = strstr(stream, ";5q") != NULL ? 5 : 1;
    params[2] = 10;
    for (mode = 0; mode < 4 && ok; ++mode) {
        pixels = NULL;
        mask = NULL;
        palette = NULL;
        flags = 0;
        w = h = nc = 0;
        if (mode == 0) {
            status = sixel_decode_raw_with_options_mask(
                (unsigned char *)(void *)stream, (int)strlen(stream),
                options, &pixels, &mask, &w, &h, &palette, &nc, &flags, NULL);
        } else if (mode == 1) {
            status = sixel_decode_direct_with_options(
                (unsigned char *)(void *)stream, (int)strlen(stream),
                options, &pixels, &w, &h, &palette, &nc, &flags, NULL);
        } else if (mode == 2) {
            memset(&result, 0, sizeof(result));
            status = sixel_decode_pixels((unsigned char const *)stream,
                strlen(stream), &opts, &result, NULL);
            pixels = result.pixels;
            w = result.width;
            h = result.height;
            flags = result.flags;
        } else {
            status = sixel_decode_direct_body_with_options(
                (unsigned char *)(void *)body, (int)strlen(body) - 2,
                options, params, 3U, &pixels, &w, &h, &palette, &nc,
                &flags, NULL);
        }
        ok = SIXEL_SUCCEEDED(status) && pixels != NULL &&
            w == width && h == height &&
            (flags & flag_mask) == expected_flags;
        for (pos = 0; pos < width * height && ok; ++pos) {
            rgb[0] = expected[pos] == 'R' ? 255 : 0;
            rgb[1] = expected[pos] == 'G' ? 255 : 0;
            rgb[2] = expected[pos] == 'B' ? 255 : 0;
            rgb[3] = expected[pos] == '.' ? 0 : 255;
            if (mode == 0) {
                ok = mask != NULL && (mask[pos] != 0) == (rgb[3] != 0);
                if (rgb[3] != 0 && ok) {
                    sample = palette + (size_t)pixels[pos] * 3U;
                    ok = memcmp(sample, rgb, 3U) == 0;
                }
            } else {
                sample = pixels + (size_t)pos * 4U;
                ok = memcmp(sample, rgb, 4U) == 0;
            }
        }
        if (!ok) {
            fprintf(stderr,
                "aspect API %d: status=%x size=%dx%d flags=%x pixel=%d: %s\n",
                mode, status, w, h, flags, pos,
                sixel_helper_get_additional_message());
        }
        free(pixels);
        free(mask);
        free(palette);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
