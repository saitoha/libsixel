/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "palette_map_test_common.h"
#include "src/decoder-parallel.h"
#include <sixel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* XOR is intentionally not idempotent, so mapping a stored value twice
 * restores the source value and fails the exact pixel checks. */
static void palette_test_map(void *context, unsigned char rgb[3],
                             int is_default)
{
    int const *enabled;

    enabled = context;
    if (*enabled == 1) {
        rgb[0] ^= is_default ? 0x55U : 0xaaU;
    }
}

int test_palette_map_case(struct palette_map_case const *specimen)
{
    char const *methods[] = {NULL, "k_undither", "lso_undither:Vlight",
                             "selective_blur"};
    sixel_palette_transform_t transform;
    sixel_palette_transform_t const *map;
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    sixel_decode_result_t plain;
    sixel_allocator_t *allocator;
    sixel_decoder_t *decoder;
    SIXELSTATUS status;
    unsigned char *stream;
    unsigned char *pixel;
    size_t body_size;
    size_t stream_size;
    size_t row_size;
    int count;
    int route;
    int method;
    int y;
    int depth;
    int ok;
    int skip;

    allocator = NULL;
    decoder = NULL;
    stream = NULL;
    ok = 0;
    skip = 0;
    memset(&result, 0, sizeof(result));
    memset(&plain, 0, sizeof(plain));
    memset(&options, 0, sizeof(options));
    options.preferred_pixelformat = specimen->rgb_output
                                        ? SIXEL_PIXELFORMAT_RGB888
                                        : SIXEL_PIXELFORMAT_RGBA8888;
    options.bgcolor[0] = 7;
    options.bgcolor[1] = 9;
    options.bgcolor[2] = 11;
    if (specimen->rgb_output) {
        options.flags = SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE;
    }
    depth = specimen->rgb_output ? 3 : 4;
    row_size = (size_t)specimen->width * (size_t)depth;
    transform.context = (void *)&specimen->mapped;
    transform.map_rgb = palette_test_map;
    map = specimen->mapped ? &transform : NULL;
    body_size = strlen(specimen->body);
    stream = malloc(body_size + 32U);
    if (stream == NULL) {
        goto end;
    }
    count = snprintf((char *)stream, body_size + 32U, "\033P%d;%dq%s\033\\",
                     specimen->params[0], specimen->params[1], specimen->body);
    if (count < 0 || (size_t)count >= body_size + 32U) {
        goto end;
    }
    stream_size = (size_t)count;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status =
        sixel_decoder_parallel_override_threads(specimen->parallel ? "2" : "1");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (route = 0; route < 3; route++) {
        for (method = 0; method < 4; method++) {
            if (method != 0 && (route != 2 || !specimen->reconstruct)) {
                continue;
            }
            if (specimen->gpu && (route != 2 || method != 2)) {
                continue;
            }
            if (route == 0) {
                status = sixel_decode_pixels_mapped(
                    stream, stream_size, &options, map, &result, allocator);
            } else if (route == 1) {
                status = sixel_decode_pixels_body_mapped(
                    (unsigned char const *)specimen->body, body_size,
                    specimen->params, 2U, &options, map, &result, allocator);
            } else {
                status = sixel_decoder_new(&decoder, allocator);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_GPU_POLICY,
                                              specimen->gpu ? "force" : "off");
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                if (methods[method] != NULL &&
                    SIXEL_FAILED(sixel_decoder_setopt(
                        decoder, SIXEL_OPTFLAG_DEQUANTIZE, methods[method]))) {
                    goto end;
                }
                status = sixel_decoder_decode_mapped(
                    decoder, stream, stream_size, &options, map, &result);
                if (specimen->gpu && status == SIXEL_FEATURE_ERROR) {
                    skip = 1;
                    goto end;
                }
            }
            if (SIXEL_FAILED(status) || result.width != specimen->width ||
                result.height != specimen->height ||
                result.stride != (int)row_size ||
                result.flags != specimen->flags) {
                fprintf(stderr,
                        "mapped route=%d method=%d status=%x "
                        "size=%dx%d stride=%d flags=%u\n",
                        route, method, status, result.width, result.height,
                        result.stride, result.flags);
                goto end;
            }
            for (y = 0; y < result.height; y++) {
                pixel = result.pixels + (size_t)y * row_size;
                if (memcmp(pixel, specimen->row, row_size) != 0) {
                    fprintf(stderr,
                            "mapped route=%d method=%d row=%d: "
                            "%u,%u,%u,%u\n",
                            route, method, y, pixel[0], pixel[1], pixel[2],
                            pixel[3]);
                    goto end;
                }
            }
            if (specimen->mapped != 1 && route == 0) {
                status = sixel_decode_pixels(stream, stream_size, &options,
                                             &plain, allocator);
                if (SIXEL_FAILED(status) || plain.flags != result.flags ||
                    plain.width != result.width ||
                    plain.height != result.height ||
                    memcmp(plain.pixels, result.pixels,
                           row_size * (size_t)result.height) != 0) {
                    fprintf(stderr, "unmapped ABI differs\n");
                    goto end;
                }
                sixel_allocator_free(allocator, plain.pixels);
                plain.pixels = NULL;
            }
            sixel_allocator_free(allocator, result.pixels);
            result.pixels = NULL;
            if (decoder != NULL) {
                sixel_decoder_unref(decoder);
                decoder = NULL;
            }
        }
    }
    ok = 1;
end:
    (void)sixel_decoder_parallel_override_threads("1");
    if (decoder != NULL) {
        sixel_decoder_unref(decoder);
    }
    if (allocator != NULL) {
        sixel_allocator_free(allocator, result.pixels);
        sixel_allocator_free(allocator, plain.pixels);
        sixel_allocator_unref(allocator);
    }
    free(stream);
    return skip ? 77 : ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
