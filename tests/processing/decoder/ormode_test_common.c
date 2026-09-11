/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/decoder-parallel.h"
#include "ormode_test_common.h"

int
test_or_decode(char const *stream, unsigned char const *expected,
               int width, int height, int threads, int direct)
{
    unsigned char const rgb[4][3] = {
        { 64, 128, 191 }, { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 }
    };
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *palette;
    SIXELSTATUS status;
    char budget[16];
    int w;
    int h;
    int colors;
    int i;
    int index;
    int ok;

    allocator = NULL;
    pixels = NULL;
    palette = NULL;
    ok = 0;
    snprintf(budget, sizeof(budget), "%d", threads);
    if (SIXEL_FAILED(sixel_decoder_parallel_override_threads(budget))) {
        goto end;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (direct) {
        status = sixel_decode_direct((unsigned char *)stream,
                                     (int)strlen(stream), &pixels,
                                     &w, &h, allocator);
    } else {
        status = sixel_decode_raw((unsigned char *)stream,
                                  (int)strlen(stream), &pixels, &w, &h,
                                  &palette, &colors, allocator);
    }
    if (SIXEL_FAILED(status) || w != width || h != height) {
        fprintf(stderr, "OR decode failed or changed dimensions\n");
        goto end;
    }
    for (i = 0; i < width * height; i++) {
        index = expected[i];
        if (direct) {
            if (index > 3 || pixels[i * 4 + 3] != 255 ||
                    memcmp(pixels + i * 4, rgb[index], 3) != 0) {
                fprintf(stderr, "OR RGBA differs at pixel %d\n", i);
                goto end;
            }
        } else if (pixels[i] != index) {
            fprintf(stderr, "OR index %u at %d, expected %d\n",
                    pixels[i], i, index);
            goto end;
        }
    }
    ok = 1;
end:
    (void)sixel_decoder_parallel_override_threads("1");
    if (allocator != NULL) {
        sixel_allocator_free(allocator, pixels);
        sixel_allocator_free(allocator, palette);
        sixel_allocator_unref(allocator);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static unsigned char g_ormode_dequantize_payload[] =
    "\033P7;5q\"1;1;8;6"
    "#0;2;100;0;0"
    "#1;2;0;100;0"
    "#1!4~!4?\033\\";

static int
ormode_dequantize_check(char const *method, sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_decoder_t *decoder;
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    unsigned char const *pixel;
    size_t index;
    size_t npixels;
    int ok;

    decoder = NULL;
    ok = 0;
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;

    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sixel_decoder_new() failed\n");
        goto end;
    }
    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_DEQUANTIZE, method);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "dequantize method %s was rejected\n", method);
        goto end;
    }
    status = sixel_decoder_decode_pixels(
        decoder,
        g_ormode_dequantize_payload,
        sizeof(g_ormode_dequantize_payload) - 1U,
        &options,
        &result);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "decode failed for %s\n", method);
        goto end;
    }
    if (result.width != 8 || result.height != 6) {
        fprintf(stderr,
                "%s: unexpected dimensions %dx%d\n",
                method,
                result.width,
                result.height);
        goto end;
    }

    npixels = (size_t)result.width * (size_t)result.height;
    for (index = 0u; index < npixels; ++index) {
        if (result.pixels[index * 4u + 3u] != 255u) {
            fprintf(stderr,
                    "%s: pixel %lu is not opaque (alpha %u)\n",
                    method,
                    (unsigned long)index,
                    (unsigned int)result.pixels[index * 4u + 3u]);
            goto end;
        }
    }
    if ((result.flags & SIXEL_DECODE_PIXELS_RESULT_ALPHA_OPAQUE) == 0U) {
        fprintf(stderr, "%s: ALPHA_OPAQUE was not reported\n", method);
        goto end;
    }

    /* Interior samples stay clear of the 3x3 kernel reaching the seam. */
    pixel = result.pixels + ((size_t)3 * 8u + 7u) * 4u;
    if (pixel[0] != 255u || pixel[1] != 0u || pixel[2] != 0u) {
        fprintf(stderr,
                "%s: unpainted OR mode cell is %u,%u,%u, expected 255,0,0\n",
                method,
                (unsigned int)pixel[0],
                (unsigned int)pixel[1],
                (unsigned int)pixel[2]);
        goto end;
    }
    pixel = result.pixels + ((size_t)3 * 8u + 0u) * 4u;
    if (pixel[0] != 0u || pixel[1] != 255u || pixel[2] != 0u) {
        fprintf(stderr,
                "%s: painted OR mode cell is %u,%u,%u, expected 0,255,0\n",
                method,
                (unsigned int)pixel[0],
                (unsigned int)pixel[1],
                (unsigned int)pixel[2]);
        goto end;
    }

    ok = 1;

end:
    if (result.pixels != NULL) {
        sixel_allocator_free(allocator, result.pixels);
    }
    if (decoder != NULL) {
        sixel_decoder_unref(decoder);
    }
    return ok;
}

int
test_or_dequant(char const *method)
{
    sixel_allocator_t *allocator;
    int ok;

    allocator = NULL;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                                         NULL, NULL))) {
        return EXIT_FAILURE;
    }
    ok = ormode_dequantize_check(method, allocator);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
