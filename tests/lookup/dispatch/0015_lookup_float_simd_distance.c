/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/README.md
 * Inspect the private kernel to verify exact keys and arithmetic directly.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include "src/colorspace.h"
#define sixel_lookup_vptree_float32_create test_float_19_create
#define sixel_lookup_vptree_float32_unref test_float_19_unref
#define sixel_lookup_vptree_float32_configure test_float_19_configure
#define sixel_lookup_vptree_float32_map test_float_19_map
#define sixel_lookup_vptree_float32_workspace_size test_float_19_workspace_size
#define sixel_lookup_vptree_float32_map_pixels test_float_19_map_pixels
#include "src/lookup-vptree-float32.c"

int
test_lookup_0019_float_simd_distance(int argc, char **argv)
{
    sixel_lookup_vptree_float32_t tree;
    double palette[256 * 3];
    float pixels[2][3];
    double lanes[3][2], actual[2], expected[2];
    int i, q, c, lane;
#if SIXEL_FLOAT_LOOKUP_SIMD
    sixel_float_distance2_t r, g, b, distance;
#endif

    (void)argc;
    (void)argv;
    fprintf(stderr, "float lookup distance: %s\n",
            SIXEL_FLOAT_LOOKUP_SIMD ? "SIMD" : "native scalar");
    memset(&tree, 0, sizeof(tree));
    tree.palette_wide = palette;
    tree.weights_wide[0] = (float)(1.0 / 10000.0);
    tree.weights_wide[1] = (float)(1.0 / 65025.0);
    tree.weights_wide[2] = 2.0f;
    for (i = 0; i < 256 * 3; ++i) {
        palette[i] = (float)((i * 137) % 769) / 255.0f - 1.0f;
    }
    for (q = 0; q < 512; ++q) {
        for (lane = 0; lane < 2; ++lane) {
            for (c = 0; c < 3; ++c) {
                pixels[lane][c] = (float)((q + c * 51) % 256) / 255;
                if (lane) {
                    pixels[lane][c] = nextafterf(pixels[lane][c], INFINITY);
                }
                if (q >= 256) {
                    pixels[lane][c] = ldexpf(pixels[lane][c] - 0.5f,
                                           q % 250 - 125);
                }
                lanes[c][lane] = pixels[lane][c];
            }
        }
        for (i = 0; i < 256; ++i) {
            for (lane = 0; lane < 2; ++lane) {
                expected[lane] = sixel_lookup_vptree_float32_pixel_distance(
                    &tree, pixels[lane], i);
            }
#if SIXEL_FLOAT_LOOKUP_SIMD && HAVE_NEON && defined(__aarch64__)
            r = vld1q_f64(lanes[0]);
            g = vld1q_f64(lanes[1]);
            b = vld1q_f64(lanes[2]);
            distance = sixel_float_distance2(&tree, r, g, b, i);
            vst1q_f64(actual, distance);
#elif SIXEL_FLOAT_LOOKUP_SIMD && HAVE_SSE2
            r = _mm_loadu_pd(lanes[0]);
            g = _mm_loadu_pd(lanes[1]);
            b = _mm_loadu_pd(lanes[2]);
            distance = sixel_float_distance2(&tree, r, g, b, i);
            _mm_storeu_pd(actual, distance);
#else
            /* The non-SIMD build still checks the scalar wide arithmetic. */
            for (lane = 0; lane < 2; ++lane) {
                actual[lane] = 0.0;
                for (c = 0; c < 3; ++c) {
                    lanes[c][lane] -= palette[i * 3 + c];
                    actual[lane] += lanes[c][lane] * lanes[c][lane]
                        * tree.weights_wide[c];
                    lanes[c][lane] = pixels[lane][c];
                }
            }
#endif
            if (memcmp(actual, expected, sizeof(actual)) != 0) {
                fprintf(stderr, "distance q=%d palette=%d\n", q, i);
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
