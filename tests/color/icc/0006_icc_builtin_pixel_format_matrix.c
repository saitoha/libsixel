/* Verify representative builtin-CMS source and destination pixel formats. */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/cms.h"

static int
icc0006_close(float actual, float expected)
{
    float delta;

    delta = actual - expected;
    if (delta < 0.0f) {
        delta = -delta;
    }
    return delta <= 0.000001f;
}

int
test_icc_0006_icc_builtin_pixel_format_matrix(int argc, char **argv)
{
    unsigned char const rgb8[3] = { 0u, 128u, 255u };
    float rgbf32[3];
    unsigned char rgb8_output[3];
    unsigned char const rgba8[4] = { 10u, 20u, 30u, 40u };
    unsigned char rgba8_output[4];
    unsigned char const gray8[3] = { 0u, 128u, 255u };
    unsigned char gray8_output[3];
    unsigned char gray_rgb8_output[9];
    sixel_cms_engine_t old_engine;
    sixel_cms_profile_t *src;
    sixel_cms_profile_t *dst;
    sixel_cms_transform_t *transform;
    int ok;

    (void)argc;
    (void)argv;
    memset(rgbf32, 0, sizeof(rgbf32));
    memset(rgb8_output, 0, sizeof(rgb8_output));
    memset(rgba8_output, 0, sizeof(rgba8_output));
    memset(gray8_output, 0, sizeof(gray8_output));
    memset(gray_rgb8_output, 0, sizeof(gray_rgb8_output));
    old_engine = sixel_cms_get_engine();
    src = NULL;
    dst = NULL;
    transform = NULL;
    ok = 0;
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    src = sixel_cms_create_srgb_profile();
    dst = sixel_cms_create_srgb_profile();
    if (src == NULL || dst == NULL) {
        goto cleanup;
    }

    transform = sixel_cms_create_transform(src,
                                           SIXEL_CMS_PIXELFORMAT_RGB_8,
                                           dst,
                                           SIXEL_CMS_PIXELFORMAT_RGB_F32,
                                           SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL ||
        !sixel_cms_do_transform(transform, rgb8, rgbf32, 1u) ||
        !icc0006_close(rgbf32[0], 0.0f) ||
        !icc0006_close(rgbf32[1], 128.0f / 255.0f) ||
        !icc0006_close(rgbf32[2], 1.0f)) {
        goto cleanup;
    }
    sixel_cms_delete_transform(transform);
    transform = NULL;

    transform = sixel_cms_create_transform(src,
                                           SIXEL_CMS_PIXELFORMAT_RGB_8,
                                           dst,
                                           SIXEL_CMS_PIXELFORMAT_RGB_8,
                                           SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL ||
        !sixel_cms_do_transform(transform, rgb8, rgb8_output, 1u) ||
        memcmp(rgb8, rgb8_output, sizeof(rgb8)) != 0) {
        goto cleanup;
    }
    sixel_cms_delete_transform(transform);
    transform = NULL;

    transform = sixel_cms_create_transform(
        src,
        SIXEL_CMS_PIXELFORMAT_GRAY_8,
        dst,
        SIXEL_CMS_PIXELFORMAT_GRAY_8,
        SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL ||
        !sixel_cms_do_transform(transform,
                                gray8,
                                gray8_output,
                                3u) ||
        memcmp(gray8, gray8_output, sizeof(gray8)) != 0) {
        goto cleanup;
    }
    sixel_cms_delete_transform(transform);
    transform = NULL;

    transform = sixel_cms_create_transform(
        src,
        SIXEL_CMS_PIXELFORMAT_GRAY_8,
        dst,
        SIXEL_CMS_PIXELFORMAT_RGB_8,
        SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL ||
        !sixel_cms_do_transform(transform,
                                gray8,
                                gray_rgb8_output,
                                3u) ||
        gray_rgb8_output[0] != 0u ||
        gray_rgb8_output[1] != 0u ||
        gray_rgb8_output[2] != 0u ||
        gray_rgb8_output[3] != 128u ||
        gray_rgb8_output[4] != 128u ||
        gray_rgb8_output[5] != 128u ||
        gray_rgb8_output[6] != 255u ||
        gray_rgb8_output[7] != 255u ||
        gray_rgb8_output[8] != 255u) {
        goto cleanup;
    }
    sixel_cms_delete_transform(transform);
    transform = NULL;

    transform = sixel_cms_create_transform(
        src,
        SIXEL_CMS_PIXELFORMAT_RGBA_8,
        dst,
        SIXEL_CMS_PIXELFORMAT_RGBA_8,
        SIXEL_CMS_TRANSFORM_COPY_ALPHA);
    if (transform == NULL ||
        !sixel_cms_do_transform(transform, rgba8, rgba8_output, 1u) ||
        memcmp(rgba8, rgba8_output, sizeof(rgba8)) != 0) {
        goto cleanup;
    }
    ok = 1;

cleanup:
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(src);
    sixel_cms_close_profile(dst);
    sixel_cms_set_engine(old_engine);
    if (!ok) {
        fprintf(stderr, "builtin CMS pixel-format matrix failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
