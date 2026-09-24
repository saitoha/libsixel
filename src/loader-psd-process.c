/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <float.h>
#include <math.h>
#include <stdint.h>
#include "allocator.h"
#include "cms.h"
#include "colorspace.h"
#include "loader-psd-color.h"
#include "loader-psd-process.h"

static int
psd_process_normalize(float *base, float *output, size_t count,
                        unsigned int channels, int width, int height,
                        unsigned char const *icc, size_t icc_size)
{
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *destination;
    sixel_cms_transform_t *transform;
    double rgb[3];
    size_t i;
    unsigned int j;
    int result;

    if (icc == NULL) {
        for (i = 0u; i < count; ++i) {
            for (j = 0u; j < 3u; ++j) {
                /* Unprofiled process colors keep the legacy canonical
                 * gamma convention. CMYK here has non-inverted inks. */
                rgb[j] = channels == 3u ? base[i * 3u + j] :
                    (1.0 - base[i * 4u + j]) *
                    (1.0 - base[i * 4u + 3u]);
            }
            if (!sixel_color_convert(rgb, rgb, SIXEL_COLORSPACE_GAMMA,
                                      SIXEL_COLORSPACE_LINEAR)) {
                return 0;
            }
            for (j = 0u; j < 3u; ++j) {
                if (!isfinite(rgb[j]) || fabs(rgb[j]) > FLT_MAX) {
                    return 0;
                }
                output[i * 3u + j] = (float)rgb[j];
            }
        }
        return 1;
    }
    source = sixel_cms_open_profile_from_mem(icc, icc_size);
    if (source == NULL) {
        return 0;
    }
    result = 0;
    destination = NULL;
    transform = NULL;
    if (channels == 3u &&
        sixel_cms_get_color_space(source) == SIXEL_CMS_COLORSPACE_RGB) {
        /* This shared entry also owns ColorSync's float preparation; do
         * not duplicate backend policy in the PSD interpretation stage. */
        result = sixel_cms_convert_profile_to_linearrgb(
            (unsigned char *)base, width, height,
            SIXEL_PIXELFORMAT_RGBFLOAT32, source);
        if (result) {
            for (i = 0u; i < count * 3u; ++i) {
                output[i] = base[i];
            }
        }
    } else if (channels == 4u &&
               sixel_cms_get_color_space(source) == SIXEL_CMS_COLORSPACE_CMYK) {
        destination = sixel_cms_create_linear_srgb_profile();
        if (destination != NULL) {
            transform = sixel_cms_create_transform(source,
                SIXEL_CMS_PIXELFORMAT_CMYK_F32, destination,
                SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
        }
        if (transform != NULL) {
            result = sixel_cms_do_transform(transform, base, output, count);
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(destination);
    sixel_cms_close_profile(source);
    return result;
}

SIXELSTATUS
sixel_psd_render_process_data(sixel_chunk_t const *chunk,
                              sixel_builtin_psd_info_t const *info,
                              int icc_status,
                              unsigned char const *icc, size_t icc_size,
                              unsigned char **pixels)
{
    sixel_psd_color_data_t data;
    SIXELSTATUS status;
    unsigned char *planes;
    float *base;
    float *output;
    double palette[SIXEL_PSD_COLOR_CHANNELS][3];
    double rgb[3];
    double sample;
    double solidity;
    size_t plane_bytes;
    size_t count;
    size_t i;
    unsigned int channels;
    unsigned int stride;
    unsigned int k;
    unsigned int j;
    int alpha;

    if (chunk == NULL || info == NULL || pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    channels = info->color_mode == 3u ? 3u :
        info->color_mode == 4u ? 4u : 0u;
    if (channels == 0u || info->channels <= channels) {
        return SIXEL_FALSE;
    }
    status = sixel_psd_read_color_data(chunk, info, &data);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    for (k = 0u; k < data.ink_count; ++k) {
        if (data.ink[k].kind == 2u) {
            break;
        }
    }
    if (k == data.ink_count && data.alternate_count == 0u) {
        return SIXEL_FALSE;
    }
    if (data.ink_count != info->channels - channels ||
        (data.identifier_present &&
         data.identifier_count != data.ink_count)) {
        goto unsupported;
    }
    status = sixel_psd_merged_alpha(chunk, info, &alpha);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    for (k = 0u; k < data.identifier_count; ++k) {
        if (data.identifier[k] == 0u) {
            alpha = 1;
        }
        for (j = 0u; j < k; ++j) {
            if (data.identifier[j] == data.identifier[k]) {
                goto unsupported;
            }
        }
    }
    if (alpha) {
        /* A merged RGB preview may already contain a white matte. Do not
         * silently double-compose it or mistake it for a saved mask. */
        sixel_helper_set_additional_message(
            "builtin PSD: process spots with merged transparency unsupported");
        return SIXEL_BAD_INPUT;
    }
    if (!sixel_psd_prepare_spots(&data, palette)) {
        goto unsupported;
    }
    if (icc_status == SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED) {
        goto unsupported;
    }
    planes = NULL;
    base = NULL;
    output = NULL;
    status = sixel_psd_read_color_planes(chunk, info, &planes, &plane_bytes);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    stride = info->depth / 8u;
    count = plane_bytes / stride;
    status = SIXEL_BAD_INTEGER_OVERFLOW;
    if (count > SIZE_MAX / (channels * sizeof(float))) {
        goto cleanup;
    }
    base = sixel_allocator_malloc(info->allocator,
                                  count * channels * sizeof(float));
    output = sixel_allocator_malloc(info->allocator,
                                    count * 3u * sizeof(float));
    if (base == NULL || output == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    status = SIXEL_BAD_INPUT;
    for (i = 0u; i < count; ++i) {
        for (j = 0u; j < channels; ++j) {
            if (!sixel_psd_color_sample(planes + j * plane_bytes + i * stride,
                                        info->depth, channels == 4u, &sample)) {
                goto cleanup;
            }
            base[i * channels + j] =
                (float)(channels == 4u ? 1.0 - sample : sample);
        }
    }
    if (!psd_process_normalize(base, output, count, channels,
            (int)info->width, (int)info->height, icc, icc_size)) {
        sixel_helper_set_additional_message(
            "builtin PSD: process color conversion failed");
        goto cleanup;
    }
    for (i = 0u; i < count; ++i) {
        for (j = 0u; j < 3u; ++j) {
            rgb[j] = output[i * 3u + j];
        }
        for (k = 0u; k < data.ink_count; ++k) {
            if (data.ink[k].kind != 2u) {
                continue;
            }
            if (!sixel_psd_color_sample(
                    planes + (channels + k) * plane_bytes + i * stride,
                    info->depth, 1, &sample)) {
                goto cleanup;
            }
            solidity = data.ink[k].opacity / 100.0;
            for (j = 0u; j < 3u; ++j) {
                rgb[j] = rgb[j] * sample + palette[k][j] *
                    (1.0 - sample) * (solidity + (1.0 - solidity) * rgb[j]);
            }
        }
        for (j = 0u; j < 3u; ++j) {
            if (!isfinite(rgb[j]) || fabs(rgb[j]) > FLT_MAX) {
                goto cleanup;
            }
            output[i * 3u + j] = (float)rgb[j];
        }
    }
    *pixels = (unsigned char *)output;
    output = NULL;
    status = SIXEL_OK;

cleanup:
    sixel_allocator_free(info->allocator, planes);
    sixel_allocator_free(info->allocator, base);
    sixel_allocator_free(info->allocator, output);
    return status;

unsupported:
    sixel_helper_set_additional_message(
        "builtin PSD: unsupported process spot definition");
    return SIXEL_BAD_INPUT;
}
