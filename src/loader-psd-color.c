/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "allocator.h"
#include "cms.h"
#include "colorspace.h"
#include "loader-psd-color.h"

static double
psd_render_lab_inverse(double value)
{
    return value > 6.0 / 29.0 ? value * value * value :
        3.0 * (6.0 / 29.0) * (6.0 / 29.0) * (value - 4.0 / 29.0);
}

static int
psd_render_lab(sixel_color_def_t const *srgb, double l, double a,
                 double b, double linear[3])
{
    double xyz[3];
    double rgb[3];
    double y;
    unsigned int j;

    /* PSD Lab is D50. The shared analytic color definition owns chromatic
     * adaptation and RGB primaries; a document Gray/RGB ICC does not apply
     * to an already colorimetric alternate-color table. */
    y = (l + 16.0) / 116.0;
    xyz[0] = 0.9642 * psd_render_lab_inverse(y + a / 500.0);
    xyz[1] = psd_render_lab_inverse(y);
    xyz[2] = 0.8249 * psd_render_lab_inverse(y - b / 200.0);
    if (!sixel_cdef_from_xyz(srgb, xyz, rgb,
            SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC)) {
        return 0;
    }
    for (j = 0u; j < 3u; ++j) {
        linear[j] = sixel_color_decode_srgb(rgb[j]);
    }
    return 1;
}

static int
psd_render_ink(sixel_color_def_t const *srgb, sixel_psd_ink_t const *ink,
                 double linear[3])
{
    double rgb[3];
    double a;
    double b;
    double h;
    double s;
    double v;
    double t;
    unsigned int sector;
    unsigned int j;

    if (ink->space == 7u) {
        a = (double)ink->component[1];
        b = (double)ink->component[2];
        if (a >= 32768.0) {
            a -= 65536.0;
        }
        if (b >= 32768.0) {
            b -= 65536.0;
        }
        if (ink->component[0] > 10000u || a < -12800.0 || a > 12700.0 ||
            b < -12800.0 || b > 12700.0) {
            return 0;
        }
        return psd_render_lab(srgb, ink->component[0] / 100.0,
                              a / 100.0, b / 100.0, linear);
    }
    if (ink->space == 0u) {
        for (j = 0u; j < 3u; ++j) {
            rgb[j] = ink->component[j] / 65535.0;
        }
    } else if (ink->space == 1u) {
        /* HSB uses a full unsigned word for each coordinate. Hue wraps
         * at one turn; its endpoint is red, not a seventh sector. */
        h = ink->component[0] * (6.0 / 65535.0);
        s = ink->component[1] / 65535.0;
        v = ink->component[2] / 65535.0;
        sector = (unsigned int)h;
        t = h - sector;
        a = v * (1.0 - s);
        b = v * (1.0 - s * (sector % 2u ? t : 1.0 - t));
        switch (sector % 6u) {
        case 0u: rgb[0] = v; rgb[1] = b; rgb[2] = a; break;
        case 1u: rgb[0] = b; rgb[1] = v; rgb[2] = a; break;
        case 2u: rgb[0] = a; rgb[1] = v; rgb[2] = b; break;
        case 3u: rgb[0] = a; rgb[1] = b; rgb[2] = v; break;
        case 4u: rgb[0] = b; rgb[1] = a; rgb[2] = v; break;
        default: rgb[0] = v; rgb[1] = a; rgb[2] = b; break;
        }
    } else if (ink->space == 2u) {
        /* Display colors have no CMYK profile of their own. Use an
         * explicit device preview: inverted C/M/Y times inverted K,
         * interpreted as sRGB. The document ICC is not an ink profile. */
        for (j = 0u; j < 3u; ++j) {
            rgb[j] = (ink->component[j] / 65535.0) *
                (ink->component[3] / 65535.0);
        }
    } else if (ink->space == 8u && ink->component[0] <= 10000u) {
        rgb[0] = rgb[1] = rgb[2] = ink->component[0] / 10000.0;
    } else {
        /* Library-specific inks require their alternate color definition;
         * do not mistake proprietary color IDs for document RGB. */
        return 0;
    }
    for (j = 0u; j < 3u; ++j) {
        linear[j] = sixel_color_decode_srgb(rgb[j]);
    }
    return 1;
}

static int
psd_render_alternates(sixel_psd_color_data_t *data)
{
    unsigned int i;
    unsigned int j;
    unsigned int k;

    if (data->alternate_count == 0u) {
        return 1;
    }
    if (data->identifier_count != data->ink_count) {
        return 0;
    }
    for (i = 0u; i < data->identifier_count; ++i) {
        for (j = 0u; j < i; ++j) {
            if (data->identifier[i] == data->identifier[j]) {
                return 0;
            }
        }
    }
    for (i = 0u; i < data->alternate_count; ++i) {
        for (j = 0u; j < i; ++j) {
            if (data->alternate_id[i] == data->alternate_id[j]) {
                return 0;
            }
        }
        for (k = 0u; k < data->ink_count; ++k) {
            if (data->alternate_id[i] == data->identifier[k]) {
                break;
            }
        }
        if (k == data->ink_count || data->ink[k].kind != 2u) {
            /* Never apply an unmatched ink to a plane by position, or
             * turn a saved alpha/mask channel into a colorant. */
            return 0;
        }
        data->ink[k].space = data->alternate[i].space;
        memcpy(data->ink[k].component, data->alternate[i].component,
               sizeof(data->ink[k].component));
        /* Solidity and channel kind still belong to DisplayInfo. */
    }
    return 1;
}

int
sixel_psd_prepare_spots(sixel_psd_color_data_t *data, double palette[][3])
{
    sixel_color_def_t *srgb;
    unsigned int k;
    int result;

    if (!psd_render_alternates(data)) {
        return 0;
    }
    srgb = sixel_cdef_name("srgb");
    if (srgb == NULL) {
        return 0;
    }
    result = 1;
    for (k = 0u; k < data->ink_count; ++k) {
        if (data->ink[k].kind == 2u &&
            !psd_render_ink(srgb, &data->ink[k], palette[k])) {
            result = 0;
            break;
        }
    }
    sixel_cdef_free(srgb);
    return result;
}

int
sixel_psd_color_sample(unsigned char const *p, unsigned int depth,
                        int bounded, double *sample)
{
    uint32_t bits;
    float value;

    if (depth == 8u) {
        *sample = p[0] / 255.0;
    } else if (depth == 16u) {
        *sample = ((unsigned int)p[0] * 256u + p[1]) / 65535.0;
    } else {
        bits = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
            (uint32_t)p[2] << 8 | p[3];
        memcpy(&value, &bits, sizeof(value));
        *sample = value;
    }
    /* Ink/table coordinates and alpha are bounded densities. Process RGB
     * can be extended; every path still rejects nonfinite source values. */
    return isfinite(*sample) &&
        (!bounded || (*sample >= 0.0 && *sample <= 1.0));
}

SIXELSTATUS
sixel_psd_render_color_data(sixel_chunk_t const *chunk,
                            sixel_builtin_psd_info_t const *info,
                            unsigned char const *bgcolor,
                            unsigned char **pixels,
                            unsigned char **mask, size_t *mask_size)
{
    sixel_psd_color_data_t data;
    sixel_color_def_t *srgb;
    SIXELSTATUS status;
    unsigned char *planes;
    unsigned char *coverage;
    float *output;
    double palette[256][3];
    double bg[3];
    double rgb[3];
    double sample;
    double alpha;
    double weight;
    double solidity;
    size_t plane_bytes;
    size_t count;
    size_t i;
    unsigned int j;
    unsigned int k;
    unsigned int low;
    unsigned int high;
    unsigned int stride;
    int duotone;

    if (chunk == NULL || info == NULL || pixels == NULL || mask == NULL ||
        mask_size == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (info->color_mode != 7u && info->color_mode != 8u) {
        return SIXEL_FALSE;
    }
    status = sixel_psd_read_color_data(chunk, info, &data);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    duotone = info->color_mode == 8u;
    if (duotone) {
        if (data.duotone_lab == NULL) {
            return SIXEL_FALSE;
        }
        if (info->channels > 2u || data.alternate_count != 0u) {
            goto unsupported;
        }
        for (k = 0u; k < data.ink_count; ++k) {
            if (data.ink[k].kind == 2u) {
                /* A spot plane is not the optional merged alpha. */
                goto unsupported;
            }
        }
    } else {
        if (data.ink_count == 0u) {
            if (data.alternate_count != 0u) {
                goto unsupported;
            }
            return SIXEL_FALSE;
        }
        for (k = 0u; k < data.ink_count; ++k) {
            if (data.ink[k].kind == 2u) {
                break;
            }
        }
        if (k == data.ink_count) {
            if (data.alternate_count != 0u) {
                goto unsupported;
            }
            if (info->channels == 3u || info->channels == 4u) {
                return SIXEL_FALSE;
            }
            goto unsupported;
        }
        if (data.ink_count != info->channels) {
            goto unsupported;
        }
        for (k = 0u; k < data.ink_count; ++k) {
            if (data.ink[k].kind != 2u) {
                goto unsupported;
            }
        }
        if (!psd_render_alternates(&data)) {
            goto unsupported;
        }
    }
    if (info->depth != 8u && info->depth != 16u && info->depth != 32u) {
        goto unsupported;
    }
    srgb = sixel_cdef_name("srgb");
    if (srgb == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (k = 0u; k < (duotone ? 256u : data.ink_count); ++k) {
        if (duotone) {
            status = psd_render_lab(srgb,
                data.duotone_lab[k * 3u] * (100.0 / 255.0),
                (double)data.duotone_lab[k * 3u + 1u] - 128.0,
                (double)data.duotone_lab[k * 3u + 2u] - 128.0,
                palette[k]) ? SIXEL_OK : SIXEL_BAD_INPUT;
        } else {
            status = psd_render_ink(srgb, &data.ink[k], palette[k])
                ? SIXEL_OK : SIXEL_BAD_INPUT;
        }
        if (SIXEL_FAILED(status)) {
            sixel_cdef_free(srgb);
            goto unsupported;
        }
    }
    sixel_cdef_free(srgb);
    planes = NULL;
    output = NULL;
    coverage = NULL;
    plane_bytes = 0u;
    status = sixel_psd_read_color_planes(chunk, info, &planes, &plane_bytes);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    stride = info->depth / 8u;
    count = plane_bytes / stride;
    if (count > SIZE_MAX / (3u * sizeof(float))) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    output = sixel_allocator_malloc(info->allocator,
                                    count * 3u * sizeof(float));
    if (output == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    if (duotone && info->channels == 2u && bgcolor == NULL) {
        coverage = sixel_allocator_malloc(info->allocator, count);
        if (coverage == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
    }
    for (j = 0u; j < 3u; ++j) {
        bg[j] = bgcolor == NULL ? 0.0 : bgcolor[j] / 255.0;
    }
    (void)sixel_color_convert(bg, bg, SIXEL_COLORSPACE_GAMMA,
                              SIXEL_COLORSPACE_LINEAR);
    for (i = 0u; i < count; ++i) {
        rgb[0] = rgb[1] = rgb[2] = 1.0;
        for (k = 0u; k < (duotone ? 1u : data.ink_count); ++k) {
            if (!sixel_psd_color_sample(
                    planes + k * plane_bytes + i * stride,
                    info->depth, 1, &sample)) {
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            if (duotone) {
                sample *= 255.0;
                low = (unsigned int)sample;
                high = low == 255u ? low : low + 1u;
                weight = sample - low;
                for (j = 0u; j < 3u; ++j) {
                    rgb[j] = palette[low][j] * (1.0 - weight) +
                        palette[high][j] * weight;
                }
            } else {
                /* A preview, not a press/ink simulation: transparent ink
                 * multiplies the underlying light; solidity interpolates
                 * toward an opaque color. Black plate samples mean ink. */
                solidity = data.ink[k].opacity / 100.0;
                for (j = 0u; j < 3u; ++j) {
                    rgb[j] = rgb[j] * sample + palette[k][j] *
                        (1.0 - sample) *
                        (solidity + (1.0 - solidity) * rgb[j]);
                }
            }
        }
        if (duotone && info->channels == 2u) {
            if (!sixel_psd_color_sample(planes + plane_bytes + i * stride,
                                        info->depth, 1, &alpha)) {
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            if (coverage != NULL) {
                coverage[i] = alpha == 0.0 ? 1u : 0u;
            }
            if (bgcolor != NULL) {
                for (j = 0u; j < 3u; ++j) {
                    rgb[j] = rgb[j] * alpha + bg[j] * (1.0 - alpha);
                }
            }
        }
        for (j = 0u; j < 3u; ++j) {
            if (!isfinite(rgb[j]) || fabs(rgb[j]) > FLT_MAX) {
                status = SIXEL_BAD_INPUT;
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
    sixel_allocator_free(info->allocator, output);
    sixel_allocator_free(info->allocator, coverage);
    return status;

unsupported:
    sixel_helper_set_additional_message(
        "builtin PSD: unsupported colorant definition");
    return SIXEL_BAD_INPUT;
}
