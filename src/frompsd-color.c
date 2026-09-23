/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

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
    size_t remaining;
    size_t name_size;
    size_t size;
    size_t old_size;
    size_t offset;
    unsigned int id;
    unsigned int count;
    int have_display;
    int have_color;

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
    have_display = 0;
    have_color = 0;
    while (remaining != 0u) {
        if (remaining >= 6u) {
            id = psd_color_u16(p + 4u);
            if (id == 1007u || id == 1077u ||
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
