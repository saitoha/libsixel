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
    size_t count;
    int c;
    unsigned int code;
    int source;
    int target;
    int budget;
    int api;
    int specimen;
    int mode;
    int first_specimen;
    int last_specimen;
    int first_budget;
    int last_budget;
    int first_api;
    int last_api;
    int first_source;
    int last_source;
    int first_target;
    int last_target;
    int ok;
    int result;
    SIXELSTATUS status;


    if (argc != 2 && argc != 7) {
        return EXIT_FAILURE;
    }
    mode = atoi(argv[1]);
    first_specimen = first_budget = first_api = 0;
    last_specimen = 1;
    last_budget = 4;
    last_api = 2;
    first_source = first_target = 0;
    last_source = last_target = 6;
    count = EXACT_FLOAT_COUNT;
    if (mode == 2) {
        if (argc != 7) return EXIT_FAILURE;
        first_specimen = atoi(argv[2]);
        first_api = atoi(argv[3]);
        first_budget = atoi(argv[4]);
        first_source = atoi(argv[5]);
        first_target = atoi(argv[6]);
        if (first_specimen < 0 || first_specimen >= 4 || first_api < 0 ||
                first_api >= 2 || first_budget < 0 || first_budget >= 4 ||
                first_source < 0 || first_source >= 6 || first_target < 0 ||
                first_target >= 6) {
            return EXIT_FAILURE;
        }
        last_specimen = first_specimen + 1;
        last_api = first_api + 1;
        last_budget = first_budget + 1;
        last_source = first_source + 1;
        last_target = first_target + 1;
        /* Each execution slot needs at least 1024 pixels for its memo.
         * A 256-key cycle leaves repeated hits in every slot. */
        count = (size_t)budgets[first_budget] * 1024U;
    }
    bytes = count * 3U * sizeof(float);
    input = (float *)malloc(bytes);
    expected = (float *)malloc(bytes);
    actual = (float *)malloc(bytes);
    result = EXIT_FAILURE;
    if (input == NULL || expected == NULL || actual == NULL) {
        goto end;
    }
    for (specimen = first_specimen; specimen < last_specimen; ++specimen) {
        for (i = 0; i < count; ++i) {
            for (c = 0; c < 3; ++c) {
                /* Every byte code occurs on every channel, with mixed
                 * triplets and repeated blocks in each worker's chunk. */
                code = (unsigned int)(i % (mode == 2 ? 256U : 1024U));
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
            input[(count - 1U) * 3U] = errors[specimen];
        }
        for (source = first_source; source < last_source; ++source) {
            for (target = first_target; target < last_target; ++target) {
                if (mode == 2) {
                    /* The prefix repeats the same 256 exact triplets.
                     * Evaluate each unique reference input once, then the
                     * exceptional last pixel. The tested API still sees
                     * the full raster and every worker's repeated hits. */
                    ok = exact_float_reference(expected, input, 256U,
                        spaces[source], spaces[target]);
                    for (i = 256U; ok && i < count; ++i) {
                        memcpy(expected + i * 3U,
                            expected + (i % 256U) * 3U, 3U * sizeof(float));
                    }
                    ok = ok && exact_float_reference(
                        expected + (count - 1U) * 3U,
                        input + (count - 1U) * 3U, 1U,
                        spaces[source], spaces[target]);
                } else {
                    ok = exact_float_reference(expected, input, count,
                        spaces[source], spaces[target]);
                }
                if (!ok) {
                    memcpy(expected, input, bytes);
                }
                for (budget = first_budget; budget < last_budget; ++budget) {
                    sixel_set_encoder_threads(budgets[budget]);
                    for (api = first_api; api < last_api; ++api) {
                        memcpy(actual, input, bytes);
                        if (api == 0) {
                            status = sixel_helper_convert_colorspace(
                                (unsigned char *)actual, bytes,
                                SIXEL_PIXELFORMAT_RGBFLOAT32,
                                spaces[source], spaces[target]);
                        } else {
                            status = sixel_color_convert_frame_rgbf32(actual,
                                count, spaces[source],
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
