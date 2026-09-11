/* Verify permissive D2B0/B2D0 routing with legacy mft2 payloads. */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/cms.h"
#include "src/compat_stub.h"

#define ICC0008_TAG_TABLE_OFFSET 128u
#define ICC0008_TAG_ENTRY_SIZE 12u

static void
icc0008_write_be16(unsigned char *bytes, uint16_t value)
{
    bytes[0] = (unsigned char)((value >> 8) & 0xffu);
    bytes[1] = (unsigned char)(value & 0xffu);
}

static void
icc0008_write_be32(unsigned char *bytes, uint32_t value)
{
    bytes[0] = (unsigned char)((value >> 24) & 0xffu);
    bytes[1] = (unsigned char)((value >> 16) & 0xffu);
    bytes[2] = (unsigned char)((value >> 8) & 0xffu);
    bytes[3] = (unsigned char)(value & 0xffu);
}

static size_t
icc0008_build_mft2_identity(unsigned char *tag, size_t capacity)
{
    size_t const input_channels = 3u;
    size_t const output_channels = 3u;
    size_t const grid_points = 2u;
    size_t clut_points;
    size_t length;
    size_t offset;
    size_t point;
    size_t channel;
    size_t shift;
    int bit;

    clut_points = 1u << input_channels;
    length = 52u +
        (input_channels * 2u +
         clut_points * output_channels +
         output_channels * 2u) * 2u;
    offset = 0u;
    point = 0u;
    channel = 0u;
    shift = 0u;
    bit = 0;
    if (tag == NULL || length > capacity) {
        return 0u;
    }

    memset(tag, 0, length);
    memcpy(tag, "mft2", 4u);
    tag[8u] = (unsigned char)input_channels;
    tag[9u] = (unsigned char)output_channels;
    tag[10u] = (unsigned char)grid_points;
    icc0008_write_be16(tag + 48u, 2u);
    icc0008_write_be16(tag + 50u, 2u);

    offset = 52u;
    for (channel = 0u; channel < input_channels; ++channel) {
        icc0008_write_be16(tag + offset, 0u);
        offset += 2u;
        icc0008_write_be16(tag + offset, 65535u);
        offset += 2u;
    }
    for (point = 0u; point < clut_points; ++point) {
        for (channel = 0u; channel < output_channels; ++channel) {
            shift = input_channels - 1u - channel;
            bit = (int)((point >> shift) & 1u);
            icc0008_write_be16(tag + offset,
                               bit != 0 ? 65535u : 0u);
            offset += 2u;
        }
    }
    for (channel = 0u; channel < output_channels; ++channel) {
        icc0008_write_be16(tag + offset, 0u);
        offset += 2u;
        icc0008_write_be16(tag + offset, 65535u);
        offset += 2u;
    }
    return length;
}

static int
icc0008_build_profile(unsigned char *profile,
                      size_t capacity,
                      size_t *profile_size)
{
    unsigned char lut[256];
    size_t lut_size;
    size_t payload_offset;
    size_t total_size;

    lut_size = 0u;
    payload_offset = ICC0008_TAG_TABLE_OFFSET + 4u +
                     ICC0008_TAG_ENTRY_SIZE * 2u;
    total_size = 0u;
    if (profile == NULL || profile_size == NULL) {
        return 0;
    }
    lut_size = icc0008_build_mft2_identity(lut, sizeof(lut));
    total_size = payload_offset + lut_size;
    if (lut_size == 0u || total_size > capacity) {
        return 0;
    }

    memset(profile, 0, capacity);
    memcpy(profile + 16u, "RGB ", 4u);
    memcpy(profile + 20u, "XYZ ", 4u);
    icc0008_write_be32(profile + ICC0008_TAG_TABLE_OFFSET, 2u);
    memcpy(profile + ICC0008_TAG_TABLE_OFFSET + 4u,
           "D2B0",
           4u);
    icc0008_write_be32(
        profile + ICC0008_TAG_TABLE_OFFSET + 8u,
        (uint32_t)payload_offset);
    icc0008_write_be32(
        profile + ICC0008_TAG_TABLE_OFFSET + 12u,
        (uint32_t)lut_size);
    memcpy(profile + ICC0008_TAG_TABLE_OFFSET + 16u,
           "B2D0",
           4u);
    icc0008_write_be32(
        profile + ICC0008_TAG_TABLE_OFFSET + 20u,
        (uint32_t)payload_offset);
    icc0008_write_be32(
        profile + ICC0008_TAG_TABLE_OFFSET + 24u,
        (uint32_t)lut_size);
    memcpy(profile + payload_offset, lut, lut_size);
    icc0008_write_be32(profile, (uint32_t)total_size);
    *profile_size = total_size;
    return 1;
}

int
test_icc_0008_icc_builtin_d2b_b2d_legacy_lut(int argc, char **argv)
{
    unsigned char src_bytes[512];
    unsigned char dst_bytes[512];
    unsigned char const input[6] = {
        0u, 127u, 255u,
        230u, 17u, 91u
    };
    unsigned char output[6];
    size_t src_size;
    size_t dst_size;
    sixel_cms_engine_t old_engine;
    sixel_cms_profile_t *src;
    sixel_cms_profile_t *dst;
    sixel_cms_transform_t *transform;
    int result;
    int step;

    (void)argc;
    (void)argv;
    memset(output, 0, sizeof(output));
    src_size = 0u;
    dst_size = 0u;
    old_engine = sixel_cms_get_engine();
    src = NULL;
    dst = NULL;
    transform = NULL;
    result = EXIT_FAILURE;
    step = 0;
    if (!icc0008_build_profile(src_bytes,
                               sizeof(src_bytes),
                               &src_size) ||
        !icc0008_build_profile(dst_bytes,
                               sizeof(dst_bytes),
                               &dst_size)) {
        goto cleanup;
    }
    step = 1;
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    if (sixel_compat_setenv("SIXEL_CMS_RENDERING_INTENT",
                            "perceptual!") != 0) {
        goto cleanup;
    }
    step = 2;
    src = sixel_cms_open_profile_from_mem(src_bytes, src_size);
    dst = sixel_cms_open_profile_from_mem(dst_bytes, dst_size);
    if (src == NULL || dst == NULL) {
        goto cleanup;
    }
    step = 3;
    transform = sixel_cms_create_transform(
        src,
        SIXEL_CMS_PIXELFORMAT_RGB_8,
        dst,
        SIXEL_CMS_PIXELFORMAT_RGB_8,
        SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL ||
        !sixel_cms_do_transform(transform, input, output, 2u) ||
        memcmp(input, output, sizeof(input)) != 0) {
        goto cleanup;
    }
    step = 4;
    result = EXIT_SUCCESS;

cleanup:
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(src);
    sixel_cms_close_profile(dst);
    (void)sixel_compat_setenv("SIXEL_CMS_RENDERING_INTENT", "");
    sixel_cms_set_engine(old_engine);
    if (result != EXIT_SUCCESS) {
        fprintf(stderr,
                "builtin D2B0/B2D0 legacy LUT routing failed at %d "
                "(%u,%u,%u,%u,%u,%u)\n",
                step,
                output[0], output[1], output[2],
                output[3], output[4], output[5]);
    }
    return result;
}
