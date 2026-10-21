/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
 * Exhaustive RGB oracle, independent of graph geometry and search order.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#define sixel_lookup_delaunay_8bit_create test_dl21_create
#define sixel_lookup_delaunay_8bit_unref test_dl21_unref
#define sixel_lookup_delaunay_8bit_configure test_dl21_configure
#define sixel_lookup_delaunay_8bit_map test_dl21_map
#define sixel_lookup_delaunay_8bit_map_pixels test_dl21_map_pixels
#include "src/lookup-delaunay-8bit.c"

int
test_lookup_0021_delaunay_byte_exact(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_lookup_delaunay_8bit_t *graph;
    unsigned char palette[300 * 3], pixel[3];
    int (*squares)[256][300];
    unsigned int state, q, limit;
    int n, kind, i, c, v, d, best, expected, actual, center, rc;

    allocator = NULL;
    graph = NULL;
    rc = EXIT_FAILURE;
    n = argc > 1 ? atoi(argv[1]) : 256;
    kind = argc > 2 ? atoi(argv[2]) : 0;
    limit = argc > 3 ? (unsigned int)strtoul(argv[3], NULL, 0) : 1U << 24;
    if (n < 1 || n > 300 || limit > (1U << 24)) {
        return EXIT_FAILURE;
    }
    /* The 900 KiB oracle table exceeds the default WebAssembly stack.
     * Keep it on the heap without changing the exhaustive comparison. */
    squares = malloc(3 * sizeof(*squares));
    if (squares == NULL) {
        return EXIT_FAILURE;
    }
    state = 1841U;
    for (i = 0; i < n; ++i) {
        for (c = 0; c < 3; ++c) {
            state = state * 1664525U + 1013904223U;
            palette[i * 3 + c] = (unsigned char)(state >> 24);
            if (kind == 1 && i >= 40) {
                palette[i * 3 + c] = palette[(i % 40) * 3 + c];
            } else if (kind == 2) {
                palette[i * 3 + c] = (unsigned char)
                    (((i >> (2 * c)) & 3) * 64);
            }
        }
    }
    if (n == 256 && kind == 0) {
        memset(palette + 255 * 3, 255, 3);
    }
    for (c = 0; c < 3; ++c) {
        for (v = 0; v < 256; ++v) {
            for (i = 0; i < n; ++i) {
                d = v - palette[i * 3 + c];
                squares[c][v][i] = d * d;
            }
        }
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))
            || SIXEL_FAILED(test_dl21_create(allocator, &graph))
            || SIXEL_FAILED(test_dl21_configure(graph, palette, n, 3))) {
        goto cleanup;
    }
    fprintf(stderr, "delaunay byte n=%d kind=%d queries=%u SIMD=%d\n",
            n, kind, limit, SIXEL_DL8_SIMD);
    for (q = 0; q < limit; ++q) {
        pixel[0] = (unsigned char)(q >> 16);
        pixel[1] = (unsigned char)(q >> 8);
        pixel[2] = (unsigned char)q;
        best = INT_MAX;
        expected = 0;
        /* Precomputed per-channel integer squares only reduce test cost;
         * every palette entry still participates in every oracle query. */
        for (i = 0; i < n; ++i) {
            d = squares[0][pixel[0]][i] + squares[1][pixel[1]][i]
                + squares[2][pixel[2]][i];
            if (d < best) {
                best = d;
                expected = i;
            }
        }
        actual = test_dl21_map(graph, pixel, -1);
        center = graph->pal2u[(q * 37U) % (unsigned int)n];
        if (actual != expected
                || sixel_lookup_delaunay_8bit_walk_scalar(graph, pixel,
                                                         center) != expected
#if SIXEL_DL8_SIMD
                || sixel_lookup_delaunay_8bit_walk_simd(graph, pixel,
                                                       center) != expected
#endif
                || test_dl21_map(graph, pixel, n + 3) != expected) {
            fprintf(stderr, "RGB=%06x expected=%d actual=%d center=%d\n",
                    q, expected, actual, center);
            goto cleanup;
        }
    }
    rc = EXIT_SUCCESS;
cleanup:
    free(squares);
    test_dl21_unref(graph);
    sixel_allocator_unref(allocator);
    return rc;
}
