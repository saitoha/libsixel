/* SPDX-License-Identifier: MIT */
/* Exact optimization equivalence, not an independent colorimetric oracle.
 * The scalar double API deliberately retains analytic TRCs and has no memo.
 * Round its outputs once to float, exactly as the unoptimized adapter did. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/colorspace.h"
#include "src/threading.h"

#define EXACT_FLOAT_COUNT 16384U

static int
exact_float_reference(float *out, float const *in, size_t count,
                       int source, int target)
{
    double input[3];
    double output[3];
    size_t i;
    int c;
    int ok;

    memcpy(out, in, count * 3U * sizeof(float));
    if (source == target) {
        return 1;
    }
    for (i = 0; i < count; ++i) {
        for (c = 0; c < 3; ++c) {
            input[c] = in[i * 3U + c];
        }
        if (source == SIXEL_COLORSPACE_SMPTEC) {
            ok = sixel_color_from_output(output, input, source, target);
        } else if (target == SIXEL_COLORSPACE_SMPTEC) {
            ok = sixel_color_convert_output(output, input, source, target);
        } else {
            ok = sixel_color_convert(output, input, source, target);
        }
        if (!ok) {
            return 0;
        }
        for (c = 0; c < 3; ++c) {
            if (!isfinite(output[c]) || fabs(output[c]) > FLT_MAX) {
                return 0;
            }
            out[i * 3U + c] = (float)output[c];
        }
    }
    return 1;
}

int
test_color_float_exact(int argc, char **argv)
{
    static int const spaces[] = {
        SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_LINEAR,
        SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_CIELAB,
        SIXEL_COLORSPACE_DIN99D, SIXEL_COLORSPACE_SMPTEC
    };
    static int const budgets[] = { 8, 1, 2, 4 };
    /* OpenVMS provides nonfinite macros through runtime values. */
    float const errors[] = { NAN, INFINITY, -INFINITY, FLT_MAX };
    float *input;
    float *expected;
    float *actual;
    float value;
    size_t i;
    size_t bytes;
    int c;
    unsigned int code;
    int source;
    int target;
    int budget;
    int api;
    int specimen;
    int mode;
    int ok;
    int result;
    SIXELSTATUS status;


    if (argc != 2) {
        return EXIT_FAILURE;
    }
    mode = atoi(argv[1]);
    bytes = EXACT_FLOAT_COUNT * 3U * sizeof(float);
    input = (float *)malloc(bytes);
    expected = (float *)malloc(bytes);
    actual = (float *)malloc(bytes);
    result = EXIT_FAILURE;
    if (input == NULL || expected == NULL || actual == NULL) {
        goto end;
    }
    for (specimen = 0; specimen < (mode == 2 ? 4 : 1); ++specimen) {
        for (i = 0; i < EXACT_FLOAT_COUNT; ++i) {
            for (c = 0; c < 3; ++c) {
                /* Every byte code occurs on every channel, with mixed
                 * triplets and repeated blocks in each worker's chunk. */
                code = (unsigned int)(i % 1024U);
                value = (float)((double)((code * (2U * c + 1U) + c * 37U
                    + (code / 256U) * (c + 1U) * 17U) % 256U) / 255.0);
                if (mode == 1) {
                    /* Adjacent floats must miss the exact grid. Include
                     * genuine 16-bit fractions, both zeros and extensions. */
                    switch ((i / 256U) % 6U) {
                    case 0: value = nextafterf(value, INFINITY); break;
                    case 1: value = nextafterf(value, -INFINITY); break;
                    case 2: value = (float)(i % 65536U) / 65535.0f; break;
                    case 3: value = value * 1.25f - .125f; break;
                    case 4: value = 0.0f; break;
                    default: value = -0.0f; break;
                    }
                }
                input[i * 3U + c] = value;
            }
        }
        if (mode == 2) {
            /* A late failure follows many memo hits. Identity conversions
             * still preserve NaN bits without validation. */
            input[(EXACT_FLOAT_COUNT - 1U) * 3U] = errors[specimen];
        }
        for (source = 0; source < 6; ++source) {
            for (target = 0; target < 6; ++target) {
                ok = exact_float_reference(expected, input, EXACT_FLOAT_COUNT,
                    spaces[source], spaces[target]);
                if (!ok) {
                    memcpy(expected, input, bytes);
                }
                for (budget = 0; budget < 4; ++budget) {
                    sixel_set_threads(budgets[budget]);
                    for (api = 0; api < 2; ++api) {
                        memcpy(actual, input, bytes);
                        if (api == 0) {
                            status = sixel_helper_convert_colorspace(
                                (unsigned char *)actual, bytes,
                                SIXEL_PIXELFORMAT_RGBFLOAT32,
                                spaces[source], spaces[target]);
                        } else {
                            status = sixel_color_convert_frame_rgbf32(actual,
                                EXACT_FLOAT_COUNT, spaces[source],
                                spaces[target]);
                        }
                        if (status != (ok ? SIXEL_OK : SIXEL_BAD_INPUT) ||
                                memcmp(actual, expected, bytes) != 0) {
                            fprintf(stderr, "exact float mode=%d case=%d "
                                "src=%d dst=%d threads=%d api=%d status=%d\n",
                                mode, specimen, spaces[source], spaces[target],
                                budgets[budget], api, status);
                            goto end;
                        }
                    }
                }
            }
        }
    }
    result = EXIT_SUCCESS;
end:
    free(actual);
    free(expected);
    free(input);
    return result;
}
