/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
 * Full-key memo collisions, missing scratch, signed zero and reversed tails.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#define sixel_lookup_delaunay_float32_create test_dl32_create
#define sixel_lookup_delaunay_float32_unref test_dl32_unref
#define sixel_lookup_delaunay_float32_configure test_dl32_configure
#define sixel_lookup_delaunay_float32_map test_dl32_map
#define sixel_lookup_delaunay_float32_map_pixels test_dl32_map_pixels
#define sixel_lookup_delaunay_float32_workspace_size test_dl32_workspace_size
#include "src/lookup-delaunay-float32.c"

int
test_lookup_0032_delaunay_float_bulk(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_lookup_delaunay_float32_t *graph;
    sixel_delaunay_float_workspace_t *scratch;
    float palette[257 * 3], pixels[257][3];
    float weights[3] = { 1.0f, 0.5f, 2.0f };
    sixel_index_t actual[257], expected[257];
    uint32_t key[3];
    unsigned int slot, collision;
    int i, c, n, count, reverse, pass, start, stride, rc;

    (void)argc;
    (void)argv;
    allocator = NULL;
    graph = NULL;
    scratch = NULL;
    rc = EXIT_FAILURE;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))
            || SIXEL_FAILED(test_dl32_create(allocator, &graph))) {
        goto cleanup;
    }
    scratch = calloc(1, sizeof(*scratch));
    if (!scratch) {
        goto cleanup;
    }
    for (i = 0; i < 257; ++i) {
        for (c = 0; c < 3; ++c) {
            palette[i * 3 + c] = (float)((i * 37 + c * 19) % 257) / 256;
            pixels[i][c] = (float)((i * 17 + c * 31) % 257) / 256;
        }
        if (i % 3 == 1) {
            memcpy(pixels[i], pixels[i - 1], sizeof(pixels[i]));
        }
    }
    pixels[0][0] = 0.0f;
    pixels[1][0] = -0.0f;
    pixels[2][0] = NAN;
    pixels[3][0] = INFINITY;
    pixels[4][0] = -FLT_MAX;
    sixel_delaunay_float_memo_key(pixels[23], key);
    slot = sixel_delaunay_float_memo_slot(key);
    for (collision = 1; collision < 1000000U; ++collision) {
        pixels[24][0] = (float)collision / 1024;
        sixel_delaunay_float_memo_key(pixels[24], key);
        if (sixel_delaunay_float_memo_slot(key) == slot) {
            break;
        }
    }
    if (collision == 1000000U) {
        goto cleanup;
    }
    for (n = 256; n <= 257; ++n) {
        if (SIXEL_FAILED(test_dl32_configure(graph, palette, n, 3, weights))) {
            goto cleanup;
        }
        if (test_dl32_workspace_size(graph)
                != (n <= 256 ? sizeof(*scratch) : 0)) {
            goto cleanup;
        }
        for (i = 0; i < 257; ++i) {
            expected[i] = (sixel_index_t)test_dl32_map(graph, pixels[i], -1);
        }
        for (reverse = 0; reverse < 2; ++reverse) {
            for (count = 0; count <= 257; ++count) {
                memset(scratch, 0, sizeof(*scratch));
                start = reverse && count ? count - 1 : 0;
                stride = reverse ? -1 : 1;
                for (pass = 0; pass < 3; ++pass) {
                    memset(actual, 0xff, sizeof(actual));
                    test_dl32_map_pixels(graph,
                        (unsigned char const *)pixels[start], (size_t)count,
                        stride * (int)sizeof(pixels[0]), actual + start,
                        stride, pass == 2 ? NULL : scratch);
                    if (memcmp(actual, expected, (size_t)count) != 0) {
                        fprintf(stderr, "bulk n=%d count=%d reverse=%d\n",
                                n, count, reverse);
                        goto cleanup;
                    }
                }
            }
        }
    }
    rc = EXIT_SUCCESS;
cleanup:
    free(scratch);
    test_dl32_unref(graph);
    sixel_allocator_unref(allocator);
    return rc;
}
