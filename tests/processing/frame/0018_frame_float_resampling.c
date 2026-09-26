/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/concepts/pixelformat-precision/README.md
 */
/* Independent small-image interpolation, without production color helpers. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/cpu.h"
#include "src/scale.h"

int
test_frame_0018_resampling(int argc, char **argv)
{
    static double const cubic_right[4] = {
        -3.0 / 16.0, 1.0 / 4.0, 3.0 / 4.0, 19.0 / 16.0
    };
    static double const down_weights[3] = {2.0 / 7.0, 3.0 / 7.0, 2.0 / 7.0};
    float pixels[27];
    float original[27];
    float scaled[48];
    sixel_allocator_t *allocator;
    double expected[48];
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    SIXELSTATUS status;
    double wx;
    double wy;
    double tolerance;
    double magnitude;
    int sw;
    int sh;
    int dw;
    int dh;
    int method;
    int invalid;
    int x;
    int y;
    int c;
    int i;
    int result;

    if (argc != 3 && argc != 4) {
        return EXIT_FAILURE;
    }
    /* A requested SIMD ceiling alone does not prove an eligible backend. */
    if (argc == 4 && strcmp(argv[3], "avx") == 0) {
#if defined(HAVE_IMMINTRIN_H) && defined(__AVX__) && \
    !defined(__i386__) && !defined(_M_IX86) && \
    (defined(__clang__) || !defined(__GNUC__))
        if (sixel_cpu_simd_level() != SIXEL_SIMD_LEVEL_AVX) {
            return 77;
        }
#else
        return 77;
#endif
    }
    if (argc == 4 && strcmp(argv[3], "sse2") == 0) {
#if defined(HAVE_SSE2) && defined(HAVE_EMMINTRIN_H) && \
    (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
        if (sixel_cpu_simd_level() != SIXEL_SIMD_LEVEL_SSE2) {
            return 77;
        }
#else
        return 77;
#endif
    }
    if (argc == 4 && strcmp(argv[3], "neon") == 0) {
#if defined(HAVE_NEON) && defined(HAVE_ARM_NEON_H) && \
    (defined(__ARM_NEON) || defined(__ARM_NEON__))
        if (sixel_cpu_simd_level() != SIXEL_SIMD_LEVEL_NEON) {
            return 77;
        }
#else
        return 77;
#endif
    }
    sw = 2;
    sh = 2;
    dw = 3;
    dh = 3;
    method = atoi(argv[2]);
    invalid = 0;
    tolerance = 2e-6;
    magnitude = 1.0;
    memset(pixels, 0, sizeof(pixels));
    memset(expected, 0, sizeof(expected));
    if (strncmp(argv[1], "tiny_", 5) == 0) {
        sw = 2;
        sh = 1;
        dw = 3;
        dh = 1;
        if (strstr(argv[1], "_y") != NULL) {
            sw = 1;
            sh = 2;
            dw = 1;
            dh = 3;
        }
        for (i = 0; i < 2; ++i) {
            for (c = 0; c < 3; ++c) {
                pixels[i * 3 + c] = ldexpf(1.0f, -149);
                if (strstr(argv[1], "mixed") != NULL) {
                    pixels[i * 3 + c] = ldexpf((float)(4 * i + c + 1), -149);
                } else if (strstr(argv[1], "zero") != NULL) {
                    pixels[i * 3 + c] = 0.0f;
                }
                if (c == 2) {
                    pixels[i * 3 + c] = -pixels[i * 3 + c];
                }
            }
        }
        for (c = 0; c < 3; ++c) {
            expected[c] = pixels[c];
            /* The center has exact weights 1/2 and 1/2. */
            expected[3 + c] = ((double)pixels[c] + pixels[3 + c]) / 2.0;
            expected[6 + c] = pixels[3 + c];
        }
        tolerance = 0.0;
    } else if (strncmp(argv[1], "identity_", 9) == 0) {
        /* A one-sample weighted mean is exactly that sample, for any tap. */
        sw = 1;
        sh = 1;
        dw = strstr(argv[1], "large") != NULL ? 6 : 2;
        dh = 1;
        if (strstr(argv[1], "_y") != NULL) {
            dh = dw;
            dw = 1;
        }
        if (strstr(argv[1], "large") != NULL) {
            pixels[0] = FLT_MAX;
            pixels[1] = -FLT_MAX;
            pixels[2] = FLT_MAX / 2.0f;
        } else if (strstr(argv[1], "subnormal") != NULL) {
            pixels[0] = ldexpf(1.0f, -149);
            pixels[1] = ldexpf(1.0f, -148);
            pixels[2] = -pixels[1];
        } else {
            pixels[0] = 0.1f;
            pixels[1] = 0.3f;
            pixels[2] = -0.9f;
        }
        for (i = 0; i < dw * dh * 3; ++i) {
            expected[i] = pixels[i % 3];
        }
        tolerance = 0.0;
    } else if (strcmp(argv[1], "constant") == 0) {
        sw = 3;
        dh = 3;
        dw = 1;
        for (i = 0; i < sw * sh; ++i) {
            pixels[i * 3] = -0.5f;
            pixels[i * 3 + 1] = 1.75f;
            pixels[i * 3 + 2] = 0.125f;
        }
        for (i = 0; i < dw * dh; ++i) {
            expected[i * 3] = -0.5;
            expected[i * 3 + 1] = 1.75;
            expected[i * 3 + 2] = 0.125;
        }
    } else if (strcmp(argv[1], "bicubic") == 0
               || strcmp(argv[1], "bounded") == 0
               || strcmp(argv[1], "large_bicubic") == 0) {
        dw = 4;
        dh = 4;
        if (strcmp(argv[1], "large_bicubic") == 0) {
            magnitude = (double)(FLT_MAX / 8.0f);
        }
        /*
         * The repository's cubic is Keys a=-1. At the outer quarter-pixel
         * center its two weights are 57/64 and -9/64, normalized by 3/4.
         * Inner weights normalize to 3/4 and 1/4. R=x, G=y, B=x*y tests
         * both axes and their intermediate values, including overshoot.
         */
        for (y = 0; y < sh; ++y) {
            for (x = 0; x < sw; ++x) {
                i = (y * sw + x) * 3;
                pixels[i] = (float)x;
                pixels[i + 1] = (float)y;
                pixels[i + 2] = (float)(x * y);
                if (magnitude != 1.0) {
                    pixels[i] = (float)((2 * x - 1) * magnitude);
                    pixels[i + 1] = (float)((2 * y - 1) * magnitude);
                    pixels[i + 2] = (float)((2 * x - 1) * (2 * y - 1)
                        * magnitude);
                }
            }
        }
        for (y = 0; y < dh; ++y) {
            for (x = 0; x < dw; ++x) {
                i = (y * dw + x) * 3;
                expected[i] = cubic_right[x];
                expected[i + 1] = cubic_right[y];
                expected[i + 2] = cubic_right[x] * cubic_right[y];
                if (magnitude != 1.0) {
                    expected[i] = 2.0 * cubic_right[x] - 1.0;
                    expected[i + 1] = 2.0 * cubic_right[y] - 1.0;
                    expected[i + 2] = expected[i] * expected[i + 1];
                }
                if (strcmp(argv[1], "bounded") == 0) {
                    /* Coverage clamps horizontally before the vertical pass. */
                    wx = fmax(0.0, fmin(1.0, cubic_right[x]));
                    expected[i] = wx;
                    expected[i + 1] = fmax(0.0, fmin(1.0, cubic_right[y]));
                    expected[i + 2] = fmax(0.0,
                        fmin(1.0, wx * cubic_right[y]));
                }
            }
        }
    } else if (strcmp(argv[1], "down") == 0) {
        sw = 3;
        sh = 3;
        dw = 1;
        dh = 1;
        /* A widened triangle over three centers gives weights 2:3:2. */
        for (y = 0; y < sh; ++y) {
            for (x = 0; x < sw; ++x) {
                i = (y * sw + x) * 3;
                pixels[i] = (float)(x * x - y * y - 1);
                pixels[i + 1] = (float)(2 * x + y * y);
                pixels[i + 2] = (float)(x * y - 2);
                for (c = 0; c < 3; ++c) {
                    expected[c] += down_weights[x] * down_weights[y]
                        * pixels[i + c];
                }
            }
        }
    } else if (strcmp(argv[1], "mixed") == 0) {
        sw = 3;
        sh = 3;
        dw = 1;
        dh = 1;
        magnitude = FLT_MAX;
        /*
         * Independent 2:3:2 weights: R=(2+3-2)/7, G=(-2+3+2)/7,
         * B=((2-3+2)/7)^2. A float R sum overflows before cancellation.
         */
        for (y = 0; y < sh; ++y) {
            for (x = 0; x < sw; ++x) {
                i = (y * sw + x) * 3;
                pixels[i] = x == 2 ? -FLT_MAX : FLT_MAX;
                pixels[i + 1] = y == 0 ? -FLT_MAX : FLT_MAX;
                pixels[i + 2] = (x + y) % 2 == 0 ? FLT_MAX : -FLT_MAX;
            }
        }
        expected[0] = 3.0 / 7.0;
        expected[1] = 3.0 / 7.0;
        expected[2] = 1.0 / 49.0;
    } else if (strcmp(argv[1], "large") == 0) {
        /* Downscale weights sum to 7/3 on both axes before normalization. */
        sw = 3;
        sh = 3;
        dw = 1;
        dh = 1;
        for (i = 0; i < sw * sh * 3; ++i) {
            pixels[i] = (i % 3 == 0) ? -FLT_MAX : FLT_MAX;
        }
        for (i = 0; i < dw * dh * 3; ++i) {
            expected[i] = (i % 3 == 0) ? -FLT_MAX : FLT_MAX;
        }
        tolerance = (double)FLT_MAX * 2e-7;
    } else if (strcmp(argv[1], "nonfinite") == 0
               || strcmp(argv[1], "overflow") == 0) {
        invalid = 1;
        for (i = 0; i < sw * sh * 3; ++i) {
            pixels[i] = (float)i / 4.0f;
        }
        if (strcmp(argv[1], "nonfinite") == 0) {
            pixels[sw * sh * 3 - 1] = (float)NAN;
        } else {
            /* A cubic negative lobe overshoots finite [0, FLT_MAX]. */
            dw = 4;
            dh = 4;
            pixels[3] = FLT_MAX;
            pixels[9] = FLT_MAX;
        }
    } else {
        /* Separable linear ramps have exact center/edge interpolation. */
        for (y = 0; y < sh; ++y) {
            for (x = 0; x < sw; ++x) {
                i = (y * sw + x) * 3;
                pixels[i] = -0.5f + 2.0f * x;
                pixels[i + 1] = 2.0f - 3.0f * y;
                pixels[i + 2] = -0.75f + x + 2.0f * y;
            }
        }
        for (y = 0; y < dh; ++y) {
            for (x = 0; x < dw; ++x) {
                wx = method == SIXEL_RES_NEAREST ? (x * sw / dw) : x / 2.0;
                wy = method == SIXEL_RES_NEAREST ? (y * sh / dh) : y / 2.0;
                i = (y * dw + x) * 3;
                expected[i] = -0.5 + 2.0 * wx;
                expected[i + 1] = 2.0 - 3.0 * wy;
                expected[i + 2] = -0.75 + wx + 2.0 * wy;
            }
        }
    }
    if (strcmp(argv[1], "bounded") == 0) {
        /* This is the internal coverage helper, not the extended frame path. */
        allocator = NULL;
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            return EXIT_FAILURE;
        }
        status = sixel_helper_scale_image_float32(scaled, pixels, sw, sh,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, dw, dh, method, allocator);
        sixel_allocator_unref(allocator);
        if (SIXEL_FAILED(status)) {
            return EXIT_FAILURE;
        }
        for (i = 0; i < dw * dh * 3; ++i) {
            if (!isfinite(scaled[i])
                    || fabs(scaled[i] - expected[i]) > tolerance) {
                fprintf(stderr, "coverage %d: %.9g expected %.17g\n", i,
                    scaled[i], expected[i]);
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }
    memcpy(original, pixels, sizeof(pixels));
    frame = NULL;
    result = EXIT_FAILURE;
    status = sixel_frame_new(&frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame, pixels, sw, sh,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    status = sixel_frame_resize_float32(frame, dw, dh, method);
    if (invalid ? status != SIXEL_BAD_INPUT : SIXEL_FAILED(status)) {
        fprintf(stderr, "resize status %x: %s\n", status,
            sixel_helper_get_additional_message());
        goto end;
    }
    status = iface->vtbl->get_pixels(iface, &view);
    if (SIXEL_FAILED(status) || view.pixels != NULL
            || view.pixels_float32 == NULL
            || view.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
            || view.colorspace != SIXEL_COLORSPACE_LINEAR
            || view.width != (invalid ? sw : dw)
            || view.height != (invalid ? sh : dh)
            || memcmp(pixels, original, sizeof(pixels)) != 0) {
        goto end;
    }
    if (invalid) {
        if (view.pixels_float32 != pixels) {
            goto end;
        }
    } else {
        for (i = 0; i < dw * dh * 3; ++i) {
            if (!isfinite(view.pixels_float32[i])
                    || fabs(view.pixels_float32[i] / magnitude - expected[i])
                        > tolerance) {
                fprintf(stderr, "component %d: %.9g expected %.17g\n", i,
                    view.pixels_float32[i], expected[i]);
                goto end;
            }
        }
    }
    result = EXIT_SUCCESS;
end:
    sixel_frame_unref(frame);
    return result;
}
