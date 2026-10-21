/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
 * Inspect every SIMD distance lane against the scalar expression's bits.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#define sixel_lookup_delaunay_float32_create test_dl31_create
#define sixel_lookup_delaunay_float32_unref test_dl31_unref
#define sixel_lookup_delaunay_float32_configure test_dl31_configure
#define sixel_lookup_delaunay_float32_map test_dl31_map
#define sixel_lookup_delaunay_float32_map_pixels test_dl31_map_pixels
#define sixel_lookup_delaunay_float32_workspace_size test_dl31_workspace_size
#include "src/lookup-delaunay-float32.c"

int
test_lookup_0031_delaunay_float_distance(int argc, char **argv)
{
    sixel_lookup_delaunay_float32_t graph;
    sixel_delaunay_float_block_t block;
    double palette[12], actual[2], expected[2], diff;
    float pixel[3];
    int q, lane, c, half;
#if SIXEL_DLF_SIMD
    sixel_delaunay_distance2_t result;
#endif

    (void)argc;
    (void)argv;
    memset(&graph, 0, sizeof(graph));
    graph.points = palette;
    graph.weights_wide[0] = 1.0f;
    graph.weights_wide[1] = 1.0f / 10000.0f;
    graph.weights_wide[2] = 1.0f / 65025.0f;
    fprintf(stderr, "delaunay distance SIMD=%d\n", SIXEL_DLF_SIMD);
    for (q = 0; q < 4000; ++q) {
        for (c = 0; c < 3; ++c) {
            pixel[c] = ldexpf((float)((q * 19 + c * 31) % 255 - 127),
                              q % 240 - 125);
        }
        for (lane = 0; lane < 4; ++lane) {
            for (c = 0; c < 3; ++c) {
                palette[lane * 3 + c] = nextafterf(ldexpf(
                    (float)((q * 37 + lane * 11 + c * 23) % 255 - 127),
                    q % 240 - 125), INFINITY);
            }
            block.x[lane] = palette[lane * 3];
            block.y[lane] = palette[lane * 3 + 1];
            block.z[lane] = palette[lane * 3 + 2];
        }
        for (half = 0; half < 4; half += 2) {
            for (lane = 0; lane < 2; ++lane) {
                expected[lane] = 0.0;
                for (c = 0; c < 3; ++c) {
                    diff = (double)pixel[c] - palette[(half + lane) * 3 + c];
                    expected[lane] += diff * diff * graph.weights_wide[c];
                }
            }
#if SIXEL_DLF_SIMD
            result = sixel_delaunay_float_distance2(&graph, pixel,
                                                     &block, half);
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
            vst1q_f64(actual, result);
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
            _mm_storeu_pd(actual, result);
#else
            wasm_v128_store(actual, result);
#endif
#else
            for (lane = 0; lane < 2; ++lane) {
                actual[lane] = sixel_delaunay_float_distance(&graph, pixel,
                                                            half + lane);
            }
#endif
            if (memcmp(actual, expected, sizeof(actual)) != 0) {
                fprintf(stderr, "distance q=%d half=%d\n", q, half);
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
