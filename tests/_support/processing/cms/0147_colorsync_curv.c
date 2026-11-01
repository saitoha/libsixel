/* SPDX-License-Identifier: MIT */
/* Independent values and native controls protect curv preparation. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/curv-samples.h"

static void
curv_input(float *pixels, int gray)
{
    int p;
    int c;

    for (p = 0; p < COLOR_CURV_SAMPLES; ++p) {
        for (c = 0; c < 3; ++c) {
            pixels[3 * p + c] =
                (float)(color_curv_source[p][gray ? 0 : c] / 65535.0);
        }
    }
}

static int
curv_values(float const *pixels, int variant)
{
    double scale;
    double expected;
    int p;
    int c;

    for (p = 0; p < COLOR_CURV_SAMPLES; ++p) {
        scale = 0.0;
        for (c = 0; c < 3; ++c) {
            expected = color_curv_profiles[variant].linear[p][c];
            if (fabs(expected) > scale) {
                scale = fabs(expected);
            }
        }
        for (c = 0; c < 3; ++c) {
            expected = color_curv_profiles[variant].linear[p][c];
            if (!isfinite(pixels[3 * p + c]) ||
                fabs(pixels[3 * p + c] - expected) >
                    scale * color_curv_relative) {
                fprintf(stderr, "%s p=%d c=%d %.17g != %.17g\n",
                    color_curv_profiles[variant].name, p, c,
                    pixels[3 * p + c], expected);
                return 0;
            }
        }
    }
    return 1;
}

/* The direct transform never invokes the source adapter. Recreating it after
 * prepared conversions detects accidental mutation of the borrowed profile. */
static int
curv_native(sixel_cms_profile_t *source, float const *input, float *output,
              int gray)
{
    sixel_cms_profile_t *destination;
    sixel_cms_transform_t *transform;
    float packed[COLOR_CURV_SAMPLES];
    int p;
    int ok;

    for (p = 0; p < COLOR_CURV_SAMPLES; ++p) {
        packed[p] = input[3 * p];
    }
    destination = sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(source, gray
        ? SIXEL_CMS_PIXELFORMAT_GRAY_F32 : SIXEL_CMS_PIXELFORMAT_RGB_F32,
        destination, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        SIXEL_CMS_TRANSFORM_DEFAULT);
    ok = transform != NULL && sixel_cms_transform_engine(transform) ==
        SIXEL_CMS_ENGINE_COLORSYNC && sixel_cms_do_transform(transform,
            gray ? packed : input, output, COLOR_CURV_SAMPLES);
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(destination);
    return ok;
}

static unsigned int
curv_be32(unsigned char const *bytes)
{
    return ((unsigned int)bytes[0] << 24) |
        ((unsigned int)bytes[1] << 16) |
        ((unsigned int)bytes[2] << 8) | bytes[3];
}

int
test_cms_0147(int argc, char **argv)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *peer;
    unsigned char bytes[1024];
    unsigned char original[1024];
    float input[COLOR_CURV_SAMPLES * 3];
    float pixels[COLOR_CURV_SAMPLES * 3];
    float expected[COLOR_CURV_SAMPLES * 3];
    float after[COLOR_CURV_SAMPLES * 3];
    unsigned int tag;
    unsigned int offset;
    size_t length;
    int variant;
    int gray;
    int strict;
    int control;
    int pass;
    int p;
    int ok;

    if (argc != 4) {
        return 1;
    }
    variant = atoi(argv[1]);
    strict = strcmp(argv[2], "strict") == 0;
    control = atoi(argv[3]);
    if (variant < 0 || variant >= COLOR_CURV_PROFILES ||
        control < 0 || control > 6) {
        return 1;
    }
    gray = color_curv_profiles[variant].gray;
    if (strict && gray) {
        return 1;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_COLORSYNC) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    length = color_curv_profiles[variant].length[0];
    memcpy(bytes, color_curv_profiles[variant].bytes[0], length);
    curv_input(input, gray);
    if (control == 3 || control == 4) {
        /* The 14-byte payload already has two physical alignment bytes.
         * Change only its declared length or padding, not the next tag. */
        for (tag = 132u; tag < 132u + 12u * curv_be32(bytes + 128);
             tag += 12u) {
            offset = curv_be32(bytes + tag + 4);
            if (memcmp(bytes + offset, "curv", 4) == 0) {
                bytes[tag + 11] = control == 3 ? 15u : 16u;
                if (control == 4) {
                    bytes[offset + 15] = 1u;
                }
            }
        }
    } else if (control >= 5) {
        /* Every pixel stays outside the curve-rewrite domain; these
         * uniform curves require no channel exchange. Keep negative dark
         * samples and -0.25, and in-range RGB neighbors in each triplet. */
        for (p = 0; p < COLOR_CURV_SAMPLES; ++p) {
            input[p * 3 + 1] = control == 5
                ? (p == 1 ? -0.25f : -0.001f) : 1.25f;
            if (gray) {
                input[p * 3] = input[p * 3 + 1];
                input[p * 3 + 2] = input[p * 3 + 1];
            }
        }
    }
    memcpy(original, bytes, length);
    source = sixel_cms_open_profile_from_mem(bytes, length);
    peer = NULL;
    ok = source != NULL;
    if (ok && (control == 1 || control >= 3)) {
        ok = curv_native(source, input, expected, gray);
    }
    if (ok && control == 1) {
        /* Opening owns its profile data; callers may reuse their byte buffer.
         * Subsequent preparation must not alter either this buffer or the
         * native source which was opened from the original ICC contents. */
        memset(bytes, 0xa5, length);
        memcpy(original, bytes, length);
    }
    if (ok && control == 2) {
        memcpy(pixels, input, sizeof(input));
        sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
        ok = !sixel_cms_convert_profile_to_linearrgb((unsigned char *)pixels,
            COLOR_CURV_SAMPLES, 1, SIXEL_PIXELFORMAT_RGBFLOAT32, source) &&
            memcmp(pixels, input, sizeof(input)) == 0 &&
            sixel_cms_get_engine() == SIXEL_CMS_ENGINE_BUILTIN;
        sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    }
    for (pass = 0; ok && pass < (control == 1 ? 3 : 1); ++pass) {
        memcpy(pixels, input, sizeof(input));
        ok = strict ? sixel_cms_convert_colorsync_linear(pixels,
            COLOR_CURV_SAMPLES, 1, 0, bytes, length)
            : sixel_cms_convert_profile_to_linearrgb((unsigned char *)pixels,
                COLOR_CURV_SAMPLES, 1, SIXEL_PIXELFORMAT_RGBFLOAT32, source);
        ok = ok && sixel_cms_get_engine() == SIXEL_CMS_ENGINE_COLORSYNC &&
            memcmp(bytes, original, length) == 0 &&
            (control >= 3 ? memcmp(pixels, expected, sizeof(pixels)) == 0
                          : curv_values(pixels, variant));
    }
    if (ok && control == 1) {
        ok = curv_native(source, input, after, gray) &&
            memcmp(expected, after, sizeof(after)) == 0;
    }
    if (ok && control == 0) {
        peer = sixel_cms_open_profile_from_mem(
            color_curv_profiles[variant].bytes[1],
            color_curv_profiles[variant].length[1]);
        memcpy(after, input, sizeof(input));
        ok = peer != NULL && sixel_cms_convert_profile_to_linearrgb(
            (unsigned char *)after, COLOR_CURV_SAMPLES, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, peer) &&
            memcmp(pixels, after, sizeof(after)) == 0;
    }
    sixel_cms_close_profile(peer);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
}
