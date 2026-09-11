/* Verify deterministic clamping of NaN and infinities at the CMS facade. */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include "src/cms.h"

static float
icc0007_float_bits(uint32_t bits)
{
    float value;

    value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

int
test_icc_0007_icc_builtin_float_special_values(int argc, char **argv)
{
    float input[18];
    float output[18];
    sixel_cms_engine_t old_engine;
    sixel_cms_profile_t *src;
    sixel_cms_profile_t *dst;
    sixel_cms_transform_t *transform;
    size_t pair;
    int ok;

    (void)argc;
    (void)argv;
    memset(input, 0, sizeof(input));
    memset(output, 0, sizeof(output));
    input[0] = icc0007_float_bits(UINT32_C(0x7fc00000));
    input[1] = 0.25f;
    input[2] = 0.75f;
    input[3] = 0.0f;
    input[4] = 0.25f;
    input[5] = 0.75f;
    input[6] = icc0007_float_bits(UINT32_C(0x7f800000));
    input[7] = 0.5f;
    input[8] = 0.25f;
    input[9] = 1.0f;
    input[10] = 0.5f;
    input[11] = 0.25f;
    input[12] = icc0007_float_bits(UINT32_C(0xff800000));
    input[13] = 0.75f;
    input[14] = 0.5f;
    input[15] = 0.0f;
    input[16] = 0.75f;
    input[17] = 0.5f;
    old_engine = sixel_cms_get_engine();
    src = NULL;
    dst = NULL;
    transform = NULL;
    pair = 0u;
    ok = 0;
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    src = sixel_cms_create_rgb_profile_from_gamma_chrm(
        2.2,
        0.3127,
        0.3290,
        0.64,
        0.33,
        0.30,
        0.60,
        0.15,
        0.06);
    dst = sixel_cms_create_srgb_profile();
    if (src == NULL || dst == NULL) {
        goto cleanup;
    }
    transform = sixel_cms_create_transform(src,
                                           SIXEL_CMS_PIXELFORMAT_RGB_F32,
                                           dst,
                                           SIXEL_CMS_PIXELFORMAT_RGB_F32,
                                           SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL ||
        !sixel_cms_do_transform(transform, input, output, 6u)) {
        goto cleanup;
    }
    for (pair = 0u; pair < 3u; ++pair) {
        if (memcmp(output + pair * 6u,
                   output + pair * 6u + 3u,
                   3u * sizeof(float)) != 0) {
            fprintf(stderr,
                    "pair %lu: %.9g %.9g %.9g / %.9g %.9g %.9g\n",
                    (unsigned long)pair,
                    output[pair * 6u + 0u],
                    output[pair * 6u + 1u],
                    output[pair * 6u + 2u],
                    output[pair * 6u + 3u],
                    output[pair * 6u + 4u],
                    output[pair * 6u + 5u]);
            goto cleanup;
        }
    }
    sixel_cms_delete_transform(transform);
    transform = NULL;
    sixel_cms_close_profile(src);
    sixel_cms_close_profile(dst);
    src = NULL;
    dst = NULL;
    input[0] = 0.5f;
    input[1] = icc0007_float_bits(UINT32_C(0x7fc00000));
    input[2] = 0.25f;
    input[3] = 0.5f;
    input[4] = 0.0f;
    input[5] = 0.25f;
    src = sixel_cms_create_cielab_d50_profile();
    dst = sixel_cms_create_srgb_profile();
    if (src == NULL || dst == NULL) {
        goto cleanup;
    }
    transform = sixel_cms_create_transform(src,
                                           SIXEL_CMS_PIXELFORMAT_LAB_F32,
                                           dst,
                                           SIXEL_CMS_PIXELFORMAT_RGB_F32,
                                           SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL ||
        !sixel_cms_do_transform(transform, input, output, 2u) ||
        memcmp(output, output + 3u, 3u * sizeof(float)) != 0) {
        goto cleanup;
    }
    ok = 1;

cleanup:
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(src);
    sixel_cms_close_profile(dst);
    sixel_cms_set_engine(old_engine);
    if (!ok) {
        fprintf(stderr, "builtin CMS float special-value clamping failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
