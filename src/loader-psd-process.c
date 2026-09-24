/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include "allocator.h"
#include "cms.h"
#include "colorspace.h"
#include "loader-psd-color.h"
#include "loader-psd-process.h"

static int
psd_process_normalize(float *base, float *output, size_t count,
                        unsigned int mode, int width, int height,
                        unsigned char const *icc, size_t icc_size)
{
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *destination;
    sixel_cms_transform_t *transform;
    sixel_cms_color_space_t domain;
    double rgb[3];
    size_t i;
    unsigned int j;
    int result;

    if (icc == NULL) {
        if (mode == 9u) {
            return sixel_psd_lab_to_linear(base, output, count);
        }
        for (i = 0u; i < count; ++i) {
            for (j = 0u; j < 3u; ++j) {
                /* Unprofiled process colors keep the legacy canonical
                 * gamma convention. CMYK here has non-inverted inks. */
                rgb[j] = mode == 1u ? base[i] :
                    mode == 3u ? base[i * 3u + j] :
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
    domain = mode == 1u ? SIXEL_CMS_COLORSPACE_GRAY :
        mode == 9u ? SIXEL_CMS_COLORSPACE_LAB :
        mode == 4u ? SIXEL_CMS_COLORSPACE_CMYK : SIXEL_CMS_COLORSPACE_RGB;
    if (sixel_cms_get_color_space(source) != domain) {
        goto cleanup;
    }
    if (mode != 4u) {
        /* This shared entry also owns ColorSync's float preparation; do
         * not duplicate backend policy in the PSD interpretation stage.
         * Gray is replicated for its existing packed-Gray adapter. */
        for (i = 0u; i < count * 3u; ++i) {
            output[i] = base[mode == 1u ? i / 3u : i];
        }
        result = sixel_cms_convert_profile_to_linearrgb(
            (unsigned char *)output, width, height,
            mode == 9u ? SIXEL_PIXELFORMAT_CIELABFLOAT32 :
                         SIXEL_PIXELFORMAT_RGBFLOAT32, source);
    } else {
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
cleanup:
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
                              unsigned char const *bgcolor,
                              unsigned char **pixels,
                              unsigned char **mask, size_t *mask_size)
{
    sixel_psd_color_data_t data;
    SIXELSTATUS status;
    unsigned char *planes;
    unsigned char *coverage;
    float *base;
    float *output;
    double palette[SIXEL_PSD_COLOR_CHANNELS][3];
    double rgb[3];
    double bg[3];
    double sample;
    double solidity;
    double opacity;
    double next_opacity;
    size_t plane_bytes;
    size_t count;
    size_t i;
    unsigned int channels;
    unsigned int stride;
    unsigned int alpha_plane;
    unsigned int display_plane;
    unsigned int k;
    unsigned int j;
    int alpha;

    if (chunk == NULL || info == NULL || pixels == NULL ||
        mask == NULL || mask_size == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    channels = info->color_mode == 1u ? 1u :
        info->color_mode == 3u || info->color_mode == 9u ? 3u :
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
    status = sixel_psd_merged_alpha(chunk, info, &alpha);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (!sixel_psd_process_layout(&data, info->channels, channels, alpha,
                                   &alpha_plane, &display_plane)) {
        goto unsupported;
    }
    alpha = alpha_plane != UINT_MAX;
    if (info->color_mode == 9u && info->depth == 32u) {
        /* The legacy Lab32 route guesses physical versus normalized units
         * from each value. Do not extend that ambiguous rule to spots. */
        goto unsupported;
    }
    if (!sixel_psd_prepare_spots(&data, palette)) {
        goto unsupported;
    }
    if (icc_status == SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED) {
        goto unsupported;
    }
    planes = NULL;
    coverage = NULL;
    base = NULL;
    output = NULL;
    status = sixel_psd_read_color_planes(chunk, info, &planes, &plane_bytes);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    stride = info->depth / 8u;
    count = plane_bytes / stride;
    status = SIXEL_BAD_INTEGER_OVERFLOW;
    if (count > SIZE_MAX / (4u * sizeof(float))) {
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
    if (alpha && bgcolor == NULL) {
        coverage = sixel_allocator_malloc(info->allocator, count);
        if (coverage == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
    }
    status = SIXEL_BAD_INPUT;
    for (i = 0u; i < count; ++i) {
        opacity = 1.0;
        if (alpha && !sixel_psd_color_sample(
                planes + alpha_plane * plane_bytes + i * stride,
                info->depth, 1, &opacity)) {
            goto cleanup;
        }
        for (j = 0u; j < channels; ++j) {
            if (!sixel_psd_color_sample(planes + j * plane_bytes + i * stride,
                                        info->depth, channels == 4u, &sample)) {
                goto cleanup;
            }
            if (info->color_mode == 9u && j != 0u) {
                /* PSD opponent words use 256 codes per Lab unit with
                 * neutral at 32768; expanding Lab8 by 257 is incorrect.
                 * L uses the full unsigned range, unlike ICC v2 PCS L.
                 * The facade stores physical a/b divided by 128. Decode
                 * before matte recovery so neutral is exactly zero. */
                sample = info->depth == 16u ?
                    (sample * 65535.0 - 32768.0) / 32768.0 :
                    (sample * 255.0 - 128.0) / 128.0;
            }
            if (alpha && opacity > 0.0) {
                /* RGB, Gray and native inverted CMYK store a white
                 * matte in each process plane. Undo it before CMS (and
                 * before CMYK inversion), not after nonlinear conversion.
                 * Subtract white first: (sample + alpha - 1) loses tiny
                 * alpha at sample == 1. At zero alpha retain hidden data;
                 * do not clip finite recovery outside the source gamut.
                 * Lab white is L=100, a=b=0, so decoded opponent
                 * coordinates have a zero matte, not an all-one matte. */
                sample = info->color_mode == 9u && j != 0u ?
                    sample / opacity : 1.0 + (sample - 1.0) / opacity;
                if (!isfinite(sample) || fabs(sample) > FLT_MAX) {
                    goto cleanup;
                }
            }
            base[i * channels + j] =
                (float)(channels == 4u ? 1.0 - sample : sample);
        }
    }
    if (!psd_process_normalize(base, output, count, info->color_mode,
            (int)info->width, (int)info->height, icc, icc_size)) {
        sixel_helper_set_additional_message(
            "builtin PSD: process color conversion failed");
        goto cleanup;
    }
    for (j = 0u; j < 3u; ++j) {
        bg[j] = bgcolor == NULL ? 0.0 : bgcolor[j] / 255.0;
    }
    (void)sixel_color_convert(bg, bg, SIXEL_COLORSPACE_GAMMA,
                              SIXEL_COLORSPACE_LINEAR);
    for (i = 0u; i < count; ++i) {
        opacity = 1.0;
        if (alpha) {
            (void)sixel_psd_color_sample(
                planes + alpha_plane * plane_bytes + i * stride,
                info->depth, 1, &opacity);
        }
        for (j = 0u; j < 3u; ++j) {
            rgb[j] = output[i * 3u + j];
            if (bgcolor != NULL) {
                rgb[j] = rgb[j] * opacity + bg[j] * (1.0 - opacity);
            }
        }
        if (bgcolor != NULL) {
            opacity = 1.0;
        }
        for (k = 0u; k < data.ink_count; ++k) {
            if (data.ink[k].kind != 2u) {
                continue;
            }
            if (!sixel_psd_color_sample(
                    planes + (display_plane + k) * plane_bytes + i * stride,
                    info->depth, 1, &sample)) {
                goto cleanup;
            }
            if (sample == 1.0) {
                continue;
            }
            solidity = data.ink[k].opacity / 100.0;
            next_opacity = opacity * sample + (1.0 - sample);
            for (j = 0u; j < 3u; ++j) {
                if (opacity == 1.0) {
                    rgb[j] = rgb[j] * sample + palette[k][j] *
                        (1.0 - sample) *
                        (solidity + (1.0 - solidity) * rgb[j]);
                } else {
                    /* Source-over with the established spot blend.
                     * Spots also cover transparent process pixels; the
                     * process alpha must not clip independent ink planes. */
                    rgb[j] = (rgb[j] * opacity * sample +
                        palette[k][j] * (1.0 - sample) *
                        (1.0 - opacity + opacity * solidity +
                         opacity * (1.0 - solidity) * rgb[j])) / next_opacity;
                }
            }
            opacity = next_opacity;
        }
        if (coverage != NULL) {
            coverage[i] = opacity == 0.0 ? 1u : 0u;
        }
        for (j = 0u; j < 3u; ++j) {
            if (!isfinite(rgb[j]) || fabs(rgb[j]) > FLT_MAX) {
                goto cleanup;
            }
            output[i * 3u + j] = (float)rgb[j];
        }
    }
    *pixels = (unsigned char *)output;
    *mask = coverage;
    *mask_size = coverage == NULL ? 0u : count;
    output = NULL;
    coverage = NULL;
    status = SIXEL_OK;

cleanup:
    sixel_allocator_free(info->allocator, planes);
    sixel_allocator_free(info->allocator, base);
    sixel_allocator_free(info->allocator, output);
    sixel_allocator_free(info->allocator, coverage);
    return status;

unsupported:
    sixel_helper_set_additional_message(
        "builtin PSD: unsupported process spot definition");
    return SIXEL_BAD_INPUT;
}
