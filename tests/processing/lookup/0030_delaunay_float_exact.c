/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
 * Independent palette-order double oracle for native float coordinates.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#define sixel_lookup_delaunay_float32_create test_dl30_create
#define sixel_lookup_delaunay_float32_unref test_dl30_unref
#define sixel_lookup_delaunay_float32_configure test_dl30_configure
#define sixel_lookup_delaunay_float32_map test_dl30_map
#define sixel_lookup_delaunay_float32_map_pixels test_dl30_map_pixels
#define sixel_lookup_delaunay_float32_workspace_size test_dl30_workspace_size
#include "src/lookup-delaunay-float32.c"

int
test_lookup_0030_delaunay_float_exact(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_lookup_delaunay_float32_t *graph;
    float palette[300 * 3], pixel[3], weights[3];
    unsigned int state;
    double distance, minimum, diff, scaled;
    int kind, n, q, i, c, expected, actual, inside, center, rc;

    (void)argc;
    (void)argv;
    allocator = NULL;
    graph = NULL;
    rc = EXIT_FAILURE;
    state = 1841;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))
            || SIXEL_FAILED(test_dl30_create(allocator, &graph))) {
        goto cleanup;
    }
    fprintf(stderr, "delaunay float exact SIMD=%d\n", SIXEL_DLF_SIMD);
    /* A subnormal site can block the geometric edge from index 0 to 2,
     * yet its rounded query distance equals index 0's. A walk that rejects
     * the higher-ID tie must still reach index 2 through an expanded cell. */
    weights[0] = 1.0f;
    weights[1] = 1.0f / 10000.0f;
    weights[2] = 1.0f / 65025.0f;
    for (c = 0; c < 3; ++c) {
        palette[c] = 0.0f;
        palette[3 + c] = nextafterf(0.0f, 1.0f);
        palette[6 + c] = c == 0 ? 3.0f : 0.0f;
        pixel[c] = c == 0 ? 2.0f : 0.0f;
    }
    if (SIXEL_FAILED(test_dl30_configure(graph, palette, 3, 3, weights))) {
        goto cleanup;
    }
    for (i = -1; i < 3; ++i) {
        if (test_dl30_map(graph, pixel, i) != 2) {
            fprintf(stderr, "rounded-distance plateau hint=%d\n", i);
            goto cleanup;
        }
    }
    for (kind = 0; kind < 8; ++kind) {
        n = kind == 0 ? 1 : kind == 1 ? 7 : kind == 7 ? 300 : 256;
        weights[0] = 1.0f;
        weights[1] = 1.0f / 10000.0f;
        weights[2] = 1.0f / 65025.0f;
        for (i = 0; i < n; ++i) {
            for (c = 0; c < 3; ++c) {
                state = state * 1664525U + 1013904223U;
                palette[i * 3 + c] = (float)(state >> 8) / 8388608.0f - 1;
                if (kind == 3) {
                    palette[i * 3 + c] *= 1.0e30f;
                } else if (kind == 4) {
                    palette[i * 3 + c] *= 1.0e-30f;
                } else if (kind == 5) {
                    palette[i * 3 + c] = (float)((i >> (2 * c)) & 3);
                }
                if (i > 0 && i % 4 == 1) {
                    palette[i * 3 + c] = palette[(i - 1) * 3 + c];
                } else if (i > 0 && i % 4 == 2) {
                    palette[i * 3 + c] = nextafterf(
                        palette[(i - 1) * 3 + c], INFINITY);
                }
            }
        }
        if (kind == 6) {
            palette[6] = NAN;
            palette[9] = INFINITY;
        }
        if (SIXEL_FAILED(test_dl30_configure(graph, palette, n, 3, weights))) {
            goto cleanup;
        }
        for (q = 0; q < 24000; ++q) {
            for (c = 0; c < 3; ++c) {
                state = state * 1664525U + 1013904223U;
                pixel[c] = (float)(state >> 8) / 2097152.0f - 4;
                if (q % 7 == 0) {
                    pixel[c] = palette[(q % n) * 3 + c];
                } else if (q % 7 == 1) {
                    pixel[c] = nextafterf(palette[(q % n) * 3 + c],
                                          INFINITY);
                } else if (q % 7 == 2) {
                    pixel[c] = (float)(((double)palette[(q % n) * 3 + c]
                        + palette[((q + 1) % n) * 3 + c]) * 0.5);
                } else if (q % 7 == 3) {
                    pixel[c] = ldexpf(pixel[c], q % 240 - 120);
                }
            }
            if (q % 101 == 0) {
                pixel[q % 3] = q % 2 ? NAN : INFINITY;
            } else if (q % 103 == 0) {
                pixel[q % 3] = -INFINITY;
            } else if (q % 107 == 0) {
                pixel[q % 3] = FLT_MAX;
            }
            expected = 0;
            minimum = DBL_MAX;
            for (i = 0; i < n; ++i) {
                distance = 0.0;
                for (c = 0; c < 3; ++c) {
                    diff = (double)pixel[c] - (double)palette[i * 3 + c];
                    distance += diff * diff * (double)weights[c];
                }
                if (distance < minimum) {
                    expected = i;
                    minimum = distance;
                }
            }
            actual = test_dl30_map(graph, pixel, -1);
            if (actual != expected
                    || test_dl30_map(graph, pixel, (q * 37) % n) != expected
                    || test_dl30_map(graph, pixel, n + 1) != expected) {
                fprintf(stderr, "kind=%d q=%d expected=%d actual=%d\n",
                        kind, q, expected, actual);
                fprintf(stderr, "pixel=%a,%a,%a hinted=%d invalid=%d\n",
                    pixel[0], pixel[1], pixel[2],
                    test_dl30_map(graph, pixel, (q * 37) % n),
                    test_dl30_map(graph, pixel, n + 1));
                goto cleanup;
            }
            inside = graph->bounded;
            for (c = 0; c < 3; ++c) {
                scaled = (double)pixel[c] * graph->scale[c];
                inside &= scaled >= graph->low[c] && scaled <= graph->high[c];
            }
            center = graph->pal2u[(q * 37) % n];
            if (inside
                    && (sixel_lookup_delaunay_float32_walk_scalar(graph,
                            pixel, center) != expected
#if SIXEL_DLF_SIMD
                        || sixel_lookup_delaunay_float32_walk_simd(graph,
                            pixel, center) != expected
#endif
                        )) {
                fprintf(stderr, "walk kind=%d q=%d center=%d\n",
                        kind, q, center);
                goto cleanup;
            }
        }
    }
    rc = EXIT_SUCCESS;
cleanup:
    test_dl30_unref(graph);
    sixel_allocator_unref(allocator);
    return rc;
}
