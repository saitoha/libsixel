/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdint.h>
#include <limits.h>
#include <string.h>
#include "chunk-view.h"
#include "frompsd-color.h"

static unsigned int
psd_color_u16(unsigned char const *p)
{
    return (unsigned int)p[0] * 256u + p[1];
}

static size_t
psd_color_u32(unsigned char const *p)
{
    return (size_t)p[0] * 16777216u + (size_t)p[1] * 65536u +
        (size_t)p[2] * 256u + p[3];
}

static int
psd_color_display(unsigned char const *p, size_t size, int modern,
                   sixel_psd_color_data_t *data)
{
    size_t stride;
    size_t count;
    size_t i;
    unsigned int j;

    stride = modern ? 13u : 14u;
    if (modern) {
        if (size < 4u || psd_color_u32(p) != 1u) {
            return 0;
        }
        p += 4u;
        size -= 4u;
    }
    count = size / stride;
    if (size % stride != 0u || count > SIXEL_PSD_COLOR_CHANNELS) {
        return 0;
    }
    for (i = 0u; i < count; ++i, p += stride) {
        data->ink[i].space = psd_color_u16(p);
        for (j = 0u; j < 4u; ++j) {
            data->ink[i].component[j] = psd_color_u16(p + 2u + j * 2u);
        }
        data->ink[i].opacity = psd_color_u16(p + 10u);
        data->ink[i].kind = p[12];
        if (data->ink[i].opacity > 100u || data->ink[i].kind > 2u) {
            return 0;
        }
    }
    data->ink_count = (unsigned int)count;
    return 1;
}

SIXELSTATUS
sixel_psd_read_color_data(sixel_chunk_t const *chunk,
                          sixel_builtin_psd_info_t const *info,
                          sixel_psd_color_data_t *data)
{
    unsigned char const *p;
    unsigned char const *old_display;
    unsigned char const *identifiers;
    size_t remaining;
    size_t name_size;
    size_t size;
    size_t old_size;
    size_t identifier_size;
    size_t offset;
    unsigned int id;
    unsigned int count;
    unsigned int i;
    unsigned int j;
    int have_display;
    int have_color;
    int have_identifiers;
    int have_alternates;

    if (chunk == NULL || info == NULL || data == NULL ||
        sixel_chunk_get_buffer(chunk) == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(data, 0, sizeof(*data));
    remaining = sixel_chunk_get_size(chunk);
    if (info->image_resources_offset > remaining ||
        info->image_resources_length >
            remaining - info->image_resources_offset) {
        return SIXEL_BAD_INPUT;
    }
    p = sixel_chunk_get_buffer(chunk) + info->image_resources_offset;
    remaining = info->image_resources_length;
    old_display = NULL;
    old_size = 0u;
    identifiers = NULL;
    identifier_size = 0u;
    have_display = 0;
    have_color = 0;
    have_identifiers = 0;
    have_alternates = 0;
    while (remaining != 0u) {
        if (remaining >= 6u) {
            id = psd_color_u16(p + 4u);
            if (id == 1007u || id == 1077u || id == 1067u ||
                (id == 1066u && info->color_mode == 8u)) {
                have_color = 1;
            }
        }
        if (remaining < 12u || memcmp(p, "8BIM", 4u) != 0) {
            goto malformed;
        }
        id = psd_color_u16(p + 4u);
        name_size = ((size_t)p[6] + 2u) & ~(size_t)1u;
        if (name_size > remaining - 10u) {
            goto malformed;
        }
        offset = 6u + name_size + 4u;
        size = psd_color_u32(p + offset - 4u);
        p += offset;
        remaining -= offset;
        if (size > remaining || (size & 1u) > remaining - size) {
            goto malformed;
        }
        if (id == 1066u && info->color_mode == 8u) {
            /* Alternate Duotone Colors is a rendered Lab lookup table,
             * not the undocumented color-mode ink/curve payload. */
            if (data->duotone_lab != NULL || size < 6u ||
                psd_color_u16(p) != 1u) {
                goto malformed;
            }
            count = psd_color_u16(p + 2u);
            offset = 4u + (size_t)count * 10u;
            if (count < 1u || count > 4u || offset > size - 2u ||
                psd_color_u16(p + offset) != 256u ||
                size - offset - 2u != 768u) {
                goto malformed;
            }
            data->duotone_lab = p + offset + 2u;
        } else if (id == 1077u) {
            if (have_display || !psd_color_display(p, size, 1, data)) {
                goto malformed;
            }
            have_display = 1;
        } else if (id == 1007u) {
            if (old_display != NULL) {
                goto malformed;
            }
            old_display = p;
            old_size = size;
        } else if (id == 1053u) {
            /* Collect valid IDs for colorants and transparency mapping.
             * An unused malformed list must not disable the legacy path. */
            have_identifiers = have_identifiers ? 2 : 1;
            identifiers = p;
            identifier_size = size;
        } else if (id == 1067u) {
            if (have_alternates || size < 4u ||
                psd_color_u16(p) != 1u) {
                goto malformed;
            }
            have_alternates = 1;
            count = psd_color_u16(p + 2u);
            if (count > SIXEL_PSD_COLOR_CHANNELS ||
                size - 4u != (size_t)count * 14u) {
                goto malformed;
            }
            data->alternate_count = count;
            for (i = 0u; i < count; ++i) {
                offset = 4u + (size_t)i * 14u;
                data->alternate_id[i] = (uint32_t)psd_color_u32(p + offset);
                data->alternate[i].space = psd_color_u16(p + offset + 4u);
                for (j = 0u; j < 4u; ++j) {
                    data->alternate[i].component[j] =
                        psd_color_u16(p + offset + 6u + j * 2u);
                }
            }
        }
        offset = size + (size & 1u);
        p += offset;
        remaining -= offset;
    }
    /* New DisplayInfo wins regardless of resource order. */
    if (!have_display && old_display != NULL &&
        !psd_color_display(old_display, old_size, 0, data)) {
        goto malformed;
    }
    data->identifier_present = have_identifiers != 0;
    if (data->alternate_count != 0u || (have_identifiers == 1 &&
        identifier_size % 4u == 0u &&
        identifier_size / 4u <= SIXEL_PSD_COLOR_CHANNELS)) {
        /* The resource length already bounds the ID list. Real PSD
         * files have no additional count word inside resource 1053. */
        if (have_identifiers != 1 || identifier_size % 4u != 0u ||
            identifier_size / 4u > SIXEL_PSD_COLOR_CHANNELS) {
            goto malformed;
        }
        data->identifier_count = (unsigned int)(identifier_size / 4u);
        for (i = 0u; i < data->identifier_count; ++i) {
            data->identifier[i] =
                (uint32_t)psd_color_u32(identifiers + i * 4u);
        }
    }
    return SIXEL_OK;

malformed:
    memset(data, 0, sizeof(*data));
    if (!have_color) {
        /* Preserve the established legacy handling of unrelated malformed
         * resources (notably ICC), before any colorant definition exists. */
        return SIXEL_FALSE;
    }
    sixel_helper_set_additional_message(
        "builtin PSD: malformed color resource");
    return SIXEL_BAD_INPUT;
}

static int
psd_color_length(unsigned char const *p, size_t remaining, int wide,
                   size_t *length)
{
    uint64_t value;

    if (remaining < (wide ? 8u : 4u)) {
        return 0;
    }
    value = psd_color_u32(p);
    if (wide) {
        value = (value << 32) | psd_color_u32(p + 4u);
    }
    if (value > SIZE_MAX) {
        return 0;
    }
    *length = (size_t)value;
    return 1;
}

int
sixel_psd_process_layout(sixel_psd_color_data_t *data,
                         unsigned int channels, unsigned int process,
                         int merged, unsigned int *alpha_plane,
                         unsigned int *display_plane)
{
    unsigned int extras;
    unsigned int offset;
    unsigned int i;
    unsigned int j;

    if (data == NULL || alpha_plane == NULL || display_plane == NULL ||
        channels <= process) {
        return 0;
    }
    extras = channels - process;
    offset = 0u;
    if (data->ink_count != extras) {
        if (!merged || data->ink_count != extras - 1u) {
            return 0;
        }
        /* Some writers omit the leading merged transparency from
         * DisplayInfo. Only explicit section metadata permits this gap. */
        offset = 1u;
    }
    *alpha_plane = merged ? process : UINT_MAX;
    *display_plane = process + offset;
    if (data->identifier_present) {
        for (i = 0u; i < data->identifier_count; ++i) {
            for (j = 0u; j < i; ++j) {
                if (data->identifier[i] == data->identifier[j]) {
                    return 0;
                }
            }
        }
        if (offset && data->identifier_count == extras) {
            if (data->identifier[0] != 0u) {
                return 0;
            }
            /* The ID list may include transparency even if DisplayInfo
             * does not. Align it once, before alternate-color lookup. */
            --data->identifier_count;
            for (i = 0u; i < data->identifier_count; ++i) {
                data->identifier[i] = data->identifier[i + 1u];
            }
        }
        if (data->identifier_count != data->ink_count) {
            return 0;
        }
        for (i = 0u; i < data->identifier_count; ++i) {
            if (data->identifier[i] == 0u) {
                if (offset || data->ink[i].kind == 2u ||
                    (merged && i != 0u)) {
                    return 0;
                }
                *alpha_plane = process + i;
            }
        }
        if (merged && !offset && data->identifier[0] != 0u) {
            return 0;
        }
    }
    if (merged && !offset && data->ink[0].kind == 2u) {
        return 0;
    }
    return 1;
}

SIXELSTATUS
sixel_psd_merged_alpha(sixel_chunk_t const *chunk,
                       sixel_builtin_psd_info_t const *info, int *present)
{
    static char const wide_keys[][5] = {
        "LMsk", "Lr16", "Lr32", "Layr", "Mt16", "Mt32", "Mtrn",
        "Alph", "FMsk", "lnk2", "FEid", "FXid", "PxSD"
    };
    unsigned char const *p;
    unsigned char const *key;
    size_t remaining;
    size_t length;
    size_t header;
    size_t padding;
    size_t i;
    int wide;

    if (chunk == NULL || info == NULL || present == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *present = 0;
    remaining = sixel_chunk_get_size(chunk);
    if (info->layer_mask_offset > remaining ||
        info->layer_mask_length > remaining - info->layer_mask_offset) {
        return SIXEL_BAD_INPUT;
    }
    remaining = info->layer_mask_length;
    if (remaining == 0u) {
        return SIXEL_OK;
    }
    p = sixel_chunk_get_buffer(chunk) + info->layer_mask_offset;
    wide = info->version == 2u;
    header = wide ? 8u : 4u;
    if (!psd_color_length(p, remaining, wide, &length) ||
        length > remaining - header || length == 1u) {
        return SIXEL_BAD_INPUT;
    }
    if (length >= 2u && psd_color_u16(p + header) >= 32768u) {
        *present = 1;
    }
    p += header + length;
    remaining -= header + length;
    if (!psd_color_length(p, remaining, 0, &length) ||
        length > remaining - 4u) {
        return SIXEL_BAD_INPUT;
    }
    p += 4u + length;
    remaining -= 4u + length;
    while (remaining != 0u) {
        if (remaining < 12u ||
            (memcmp(p, "8BIM", 4u) != 0 && memcmp(p, "8B64", 4u) != 0)) {
            return SIXEL_BAD_INPUT;
        }
        key = p + 4u;
        wide = memcmp(p, "8B64", 4u) == 0;
        if (info->version == 2u) {
            for (i = 0u; i < sizeof(wide_keys) / sizeof(wide_keys[0]); ++i) {
                if (memcmp(key, wide_keys[i], 4u) == 0) {
                    wide = 1;
                }
            }
        }
        header = wide ? 16u : 12u;
        if (!psd_color_length(p + 8u, remaining - 8u, wide, &length) ||
            length > remaining - header) {
            return SIXEL_BAD_INPUT;
        }
        if (memcmp(key, "Mtrn", 4u) == 0 ||
            memcmp(key, "Mt16", 4u) == 0 ||
            memcmp(key, "Mt32", 4u) == 0) {
            *present = 1;
        } else if ((memcmp(key, "Lr16", 4u) == 0 ||
                    memcmp(key, "Lr32", 4u) == 0 ||
                    memcmp(key, "Layr", 4u) == 0) && length >= 2u &&
                   psd_color_u16(p + header) >= 32768u) {
            *present = 1;
        }
        /* Global tagged blocks are padded to four bytes. */
        padding = (4u - length % 4u) % 4u;
        if (padding > remaining - header - length) {
            return SIXEL_BAD_INPUT;
        }
        p += header + length + padding;
        remaining -= header + length + padding;
    }
    return SIXEL_OK;
}
