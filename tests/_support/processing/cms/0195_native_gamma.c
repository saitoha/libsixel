/* SPDX-License-Identifier: MIT */
/* Compare compatibility mode with this OS's original-tag native transform. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/compat_stub.h"
#include "src/options.h"
#include "src/options-registry.h"
#include "tests/data/color-numeric/curv-samples.h"
#include "tests/data/color-numeric/rgb-power.h"
#include "tests/data/color-numeric/gray-samples.h"

#define NATIVE_GAMMA_MAX 32

static unsigned int
ng_read32(unsigned char const *bytes)
{
    return ((unsigned int)bytes[0] << 24) |
        ((unsigned int)bytes[1] << 16) |
        ((unsigned int)bytes[2] << 8) | bytes[3];
}

/* Swap directory references, leaving every original tag payload untouched.
 * This independent ICC-byte operation does not call production preparation
 * or rebuild curves from their exponents. It handles unequal payload sizes. */
static int
ng_permute(unsigned char *profile, size_t length, float *pixels,
              int count, int exchange)
{
    unsigned char entry[8];
    float sample;
    size_t xyz[3];
    size_t trc[3];
    size_t i;
    size_t c;
    unsigned int n;
    int p;

    if (exchange == 3) {
        return 1;
    }
    if (exchange < 0 || exchange > 1 || length < 132u) {
        return 0;
    }
    memset(xyz, 0, sizeof(xyz));
    memset(trc, 0, sizeof(trc));
    n = ng_read32(profile + 128);
    if (n > (length - 132u) / 12u) {
        return 0;
    }
    for (i = 0; i < n; ++i) {
        for (c = 0; c < 3; ++c) {
            if (memcmp(profile + 132u + 12u * i,
                       c == 0 ? "rXYZ" : c == 1 ? "gXYZ" : "bXYZ", 4) == 0) {
                xyz[c] = 136u + 12u * i;
            }
            if (memcmp(profile + 132u + 12u * i,
                       c == 0 ? "rTRC" : c == 1 ? "gTRC" : "bTRC", 4) == 0) {
                trc[c] = 136u + 12u * i;
            }
        }
    }
    for (c = 0; c < 3; ++c) {
        if (xyz[c] == 0u || trc[c] == 0u) {
            return 0;
        }
    }
    memcpy(entry, profile + xyz[exchange], sizeof(entry));
    memcpy(profile + xyz[exchange], profile + xyz[2], sizeof(entry));
    memcpy(profile + xyz[2], entry, sizeof(entry));
    memcpy(entry, profile + trc[exchange], sizeof(entry));
    memcpy(profile + trc[exchange], profile + trc[2], sizeof(entry));
    memcpy(profile + trc[2], entry, sizeof(entry));
    memset(profile + 84, 0, 16);
    for (p = 0; p < count; ++p) {
        memcpy(&sample, pixels + p * 3 + exchange, sizeof(sample));
        memcpy(pixels + p * 3 + exchange, pixels + p * 3 + 2, sizeof(sample));
        memcpy(pixels + p * 3 + 2, &sample, sizeof(sample));
    }
    return 1;
}

/* Expected payloads come from the independently permuted serialized ICC,
 * not the production adapter or its mode label. Only the compatibility
 * conversion interval is compared with these records by the TAP test. */
static int
ng_expected(unsigned char const *profile, size_t length,
               float const *pixels, int count, int gray)
{
    static char const *const names[8] = {
        "rTRC", "gTRC", "bTRC", "rXYZ", "gXYZ", "bXYZ", "wtpt", "kTRC"
    };
    unsigned int n;
    unsigned int offset;
    unsigned int size;
    unsigned int bits;
    unsigned int i;
    unsigned int j;
    unsigned int k;
    int p;

    if (length < 132u) {
        return 0;
    }
    fprintf(stderr, "NGEXPECT|begin\n");
    n = ng_read32(profile + 128);
    for (i = gray ? 6u : 0u; i < (gray ? 8u : 7u); ++i) {
        for (j = 0u; j < n; ++j) {
            if (memcmp(profile + 132u + 12u * j, names[i], 4) == 0) {
                offset = ng_read32(profile + 136u + 12u * j);
                size = ng_read32(profile + 140u + 12u * j);
                if (offset > length || size > length - offset) {
                    return 0;
                }
                fprintf(stderr, "libsixel[loader_contract]: "
                    "LSXCMS3|event=gamma-source|tag=%s|bytes=", names[i]);
                for (k = 0u; k < size; ++k) {
                    fprintf(stderr, "%02x", profile[offset + k]);
                }
                fputc('\n', stderr);
                break;
            }
        }
        if (j == n) {
            return 0;
        }
    }
    fprintf(stderr, "libsixel[loader_contract]: "
        "LSXCMS3|event=gamma-source|samples=");
    for (p = 0; p < (gray ? count : count * 3); ++p) {
        memcpy(&bits, pixels + (gray ? p * 3 : p), sizeof(bits));
        fprintf(stderr, "%08x", bits);
    }
    fprintf(stderr, "\nNGEXPECT|end\n");
    return 1;
}

static int
ng_native(sixel_cms_profile_t *source, float const *input, float *output,
             int count, int gray, sixel_cms_engine_t engine)
{
    sixel_cms_profile_t *destination;
    sixel_cms_transform_t *transform;
    float packed[NATIVE_GAMMA_MAX];
    int p;
    int ok;

    for (p = 0; p < count; ++p) {
        packed[p] = input[p * 3];
    }
    destination = sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(source, gray
        ? SIXEL_CMS_PIXELFORMAT_GRAY_F32 : SIXEL_CMS_PIXELFORMAT_RGB_F32,
        destination, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        SIXEL_CMS_TRANSFORM_DEFAULT);
    ok = transform != NULL && sixel_cms_transform_engine(transform) == engine
        && sixel_cms_do_transform(transform, gray ? packed : input,
                                 output, (size_t)count);
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(destination);
    return ok;
}

static int
ng_convert(sixel_cms_profile_t *source, unsigned char const *bytes,
              size_t length, float *pixels, int count, int strict)
{
    return strict ? sixel_cms_convert_colorsync_linear(pixels, count, 1,
        0, bytes, length) : sixel_cms_convert_profile_to_linearrgb(
        (unsigned char *)pixels, count, 1, SIXEL_PIXELFORMAT_RGBFLOAT32,
        source);
}

int
test_cms_0195(int argc, char **argv)
{
    sixel_option_argument_schema_t const *schema;
    sixel_cms_engine_t previous;
    sixel_cms_engine_t engine;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *reference;
    unsigned char bytes[1024];
    unsigned char original[1024];
    unsigned char permuted[1024];
    float input[NATIVE_GAMMA_MAX * 3];
    float ordered[NATIVE_GAMMA_MAX * 3];
    float expected[NATIVE_GAMMA_MAX * 3];
    float before[NATIVE_GAMMA_MAX * 3];
    float borrowed[NATIVE_GAMMA_MAX * 3];
    float after[NATIVE_GAMMA_MAX * 3];
    float pixels[NATIVE_GAMMA_MAX * 3];
    size_t length;
    size_t pixel_bytes;
    int variant;
    int gray;
    int strict;
    int exchange;
    int count;
    int p;
    int c;
    int ok;
    int control;

    if (argc != 7 || !sixel_cms_engine_from_string(argv[6], &engine)) {
        return 1;
    }
    variant = atoi(argv[2]);
    strict = strcmp(argv[3], "strict") == 0;
    exchange = atoi(argv[4]);
    control = atoi(argv[5]);
    gray = 0;
    if (strcmp(argv[1], "curv") == 0) {
        if (variant < 0 || variant >= COLOR_CURV_PROFILES) {
            return 1;
        }
        gray = color_curv_profiles[variant].gray;
        length = color_curv_profiles[variant].length[0];
        memcpy(bytes, color_curv_profiles[variant].bytes[0], length);
        count = COLOR_CURV_SAMPLES;
        for (p = 0; p < count; ++p) {
            for (c = 0; c < 3; ++c) {
                input[p * 3 + c] =
                    (float)(color_curv_source[p][gray ? 0 : c] / 65535.0);
            }
        }
    } else if (strcmp(argv[1], "power") == 0) {
        if (variant < 0 || variant >= COLOR_RGB_POWER_PROFILES) {
            return 1;
        }
        length = color_rgb_power_profiles[variant].length;
        memcpy(bytes, color_rgb_power_profiles[variant].bytes, length);
        count = COLOR_RGB_POWER_SAMPLES;
        for (p = 0; p < count; ++p) {
            for (c = 0; c < 3; ++c) {
                input[p * 3 + c] =
                    (float)(color_rgb_power_source[p][c] / 65535.0);
            }
        }
    } else if (strcmp(argv[1], "gray") == 0) {
        gray = 1;
        length = sizeof(color_gray_icc);
        memcpy(bytes, color_gray_icc, length);
        count = COLOR_GRAY_SAMPLE_COUNT;
        for (p = 0; p < count; ++p) {
            for (c = 0; c < 3; ++c) {
                input[p * 3 + c] = (float)(color_gray_source[p] / 65535.0);
            }
        }
    } else {
        return 1;
    }
    if (count > NATIVE_GAMMA_MAX || (strict && gray) ||
        control < 0 || control > 2) {
        return 1;
    }
    pixel_bytes = (size_t)count * 3u * sizeof(float);
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(engine);
    if (sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    schema = sixel_option_registry_get(
        SIXEL_OPTION_SCHEMA_COLORSYNC_NATIVE_GAMMA);
    source = NULL;
    reference = NULL;
    memcpy(original, bytes, length);
    memcpy(permuted, bytes, length);
    memcpy(ordered, input, pixel_bytes);
    ok = schema != NULL && ng_permute(permuted, length, ordered,
                                      count, exchange);
    if (ok) {
        source = sixel_cms_open_profile_from_mem(bytes, length);
        reference = sixel_cms_open_profile_from_mem(permuted, length);
        ok = source != NULL && reference != NULL &&
            ng_native(reference, ordered, expected, count, gray, engine);
    }
    if (ok && control == 1) {
        ok = ng_native(source, input, borrowed, count, gray, engine);
    }
    /* The test owns this process environment. The borrowed profile is opened
     * before toggles, so open-time caching cannot satisfy the reuse case. */
    if (ok) {
        ok = sixel_compat_setenv(schema->env_name, "0") == 0;
        memcpy(before, input, pixel_bytes);
        ok = ok && ng_convert(source, bytes, length, before, count, strict);
    }
    if (ok) {
        ok = sixel_compat_setenv(schema->env_name,
                                control == 2 ? "invalid" : "1") == 0;
        memcpy(pixels, input, pixel_bytes);
        if (control != 2 && engine == SIXEL_CMS_ENGINE_COLORSYNC) {
            ok = ok && ng_expected(permuted, length, ordered, count, gray);
        }
        fprintf(stderr, "NGPHASE|begin\n");
        if (control == 2 && engine == SIXEL_CMS_ENGINE_COLORSYNC) {
            ok = ok && !ng_convert(source, bytes, length, pixels, count,
                                   strict) &&
                memcmp(pixels, input, pixel_bytes) == 0 &&
                strcmp(sixel_helper_get_additional_message(),
                    "SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA must be 0 or 1.")
                    == 0;
        } else {
            ok = ok && ng_convert(source, bytes, length, pixels, count,
                                  strict) &&
                memcmp(pixels, engine == SIXEL_CMS_ENGINE_COLORSYNC
                    ? expected : before, pixel_bytes) == 0;
        }
        fprintf(stderr, "NGPHASE|end\n");
        ok = ok && memcmp(bytes, original, length) == 0 &&
            sixel_cms_get_engine() == engine;
    }
    if (ok && control == 1) {
        ok = sixel_compat_setenv(schema->env_name, "0") == 0;
        memcpy(after, input, pixel_bytes);
        ok = ok && ng_convert(source, bytes, length, after, count, strict) &&
            memcmp(before, after, pixel_bytes) == 0 &&
            ng_native(source, input, after, count, gray, engine) &&
            memcmp(borrowed, after, pixel_bytes) == 0 &&
            memcmp(bytes, original, length) == 0;
    }
    if (schema != NULL) {
        (void)sixel_compat_setenv(schema->env_name, "");
    }
    sixel_cms_close_profile(reference);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
}
