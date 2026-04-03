/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>

#include "fuzz-loader-builtin-struct-common.h"

static unsigned char
fuzz_pick_png_plte_bit_depth(unsigned char selector)
{
    static unsigned char const depths[] = {1u, 2u, 4u, 8u};

    return depths[selector % (sizeof(depths) / sizeof(depths[0]))];
}

static int
fuzz_append_plte_chunk(fuzz_cursor_t *cursor,
                       fuzz_byte_buffer_t *payload,
                       unsigned char bit_depth,
                       size_t *palette_entries)
{
    fuzz_byte_buffer_t plte;
    size_t max_entries;
    size_t entries;
    size_t i;

    if (cursor == NULL || payload == NULL || palette_entries == NULL) {
        return 0;
    }

    max_entries = (size_t)1u << bit_depth;
    if (max_entries == 0u || max_entries > 256u) {
        max_entries = 256u;
    }

    entries = 1u + (size_t)(fuzz_cursor_take_u8(cursor, 0u) % max_entries);

    fuzz_byte_buffer_init(&plte);
    for (i = 0u; i < entries; ++i) {
        if (!fuzz_byte_buffer_append_u8(&plte,
                                        fuzz_cursor_take_u8(cursor,
                                                            (unsigned char)(i & 0xffu))) ||
            !fuzz_byte_buffer_append_u8(&plte,
                                        fuzz_cursor_take_u8(cursor,
                                                            (unsigned char)((i * 3u) & 0xffu))) ||
            !fuzz_byte_buffer_append_u8(&plte,
                                        fuzz_cursor_take_u8(cursor,
                                                            (unsigned char)((i * 5u) & 0xffu)))) {
            fuzz_byte_buffer_reset(&plte);
            return 0;
        }
    }

    if (!fuzz_append_png_chunk(payload,
                               "PLTE",
                               plte.data,
                               plte.size,
                               (int)(fuzz_cursor_take_u8(cursor, 0u) & 0x01u),
                               (uint32_t)fuzz_cursor_take_u32be(cursor, 1u))) {
        fuzz_byte_buffer_reset(&plte);
        return 0;
    }

    fuzz_byte_buffer_reset(&plte);
    *palette_entries = entries;
    return 1;
}

static int
fuzz_append_trns_chunk(fuzz_cursor_t *cursor,
                       fuzz_byte_buffer_t *payload,
                       unsigned char color_type,
                       size_t palette_entries)
{
    unsigned char trns[1024];
    size_t trns_size;
    size_t i;

    if (cursor == NULL || payload == NULL) {
        return 0;
    }

    trns_size = 0u;
    switch (color_type) {
    case 0u:
        trns_size = (fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u ? 2u :
            (size_t)(1u + (fuzz_cursor_take_u8(cursor, 0u) % 6u));
        break;
    case 2u:
        trns_size = (fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u ? 6u :
            (size_t)(1u + (fuzz_cursor_take_u8(cursor, 0u) % 10u));
        break;
    case 3u:
    default:
        if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) == 0u) {
            trns_size = palette_entries;
        } else {
            trns_size = palette_entries +
                (size_t)(fuzz_cursor_take_u8(cursor, 0u) % 17u);
        }
        break;
    }

    if (trns_size > sizeof(trns)) {
        trns_size = sizeof(trns);
    }

    for (i = 0u; i < trns_size; ++i) {
        trns[i] = fuzz_cursor_take_u8(cursor, (unsigned char)(i & 0xffu));
    }

    return fuzz_append_png_chunk(payload,
                                 "tRNS",
                                 trns,
                                 trns_size,
                                 (int)(fuzz_cursor_take_u8(cursor, 0u) & 0x01u),
                                 (uint32_t)fuzz_cursor_take_u32be(cursor, 1u));
}

static int
fuzz_append_hist_chunk(fuzz_cursor_t *cursor,
                       fuzz_byte_buffer_t *payload,
                       size_t palette_entries)
{
    fuzz_byte_buffer_t hist;
    size_t entries;
    size_t i;

    if (cursor == NULL || payload == NULL || palette_entries == 0u) {
        return 0;
    }

    entries = palette_entries;
    if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u) {
        entries = 1u + (size_t)(fuzz_cursor_take_u8(cursor, 0u) % 32u);
    }

    fuzz_byte_buffer_init(&hist);
    for (i = 0u; i < entries; ++i) {
        if (!fuzz_byte_buffer_append_u16be(&hist,
                                           (uint16_t)fuzz_cursor_take_u16be(cursor, 0u))) {
            fuzz_byte_buffer_reset(&hist);
            return 0;
        }
    }

    if (!fuzz_append_png_chunk(payload,
                               "hIST",
                               hist.data,
                               hist.size,
                               (int)(fuzz_cursor_take_u8(cursor, 0u) & 0x01u),
                               (uint32_t)fuzz_cursor_take_u32be(cursor, 1u))) {
        fuzz_byte_buffer_reset(&hist);
        return 0;
    }

    fuzz_byte_buffer_reset(&hist);
    return 1;
}

static int
fuzz_append_bkgd_chunk(fuzz_cursor_t *cursor,
                       fuzz_byte_buffer_t *payload,
                       unsigned char color_type)
{
    unsigned char bkgd[6];
    size_t bkgd_size;

    if (cursor == NULL || payload == NULL) {
        return 0;
    }

    switch (color_type) {
    case 0u:
        bkgd_size = 2u;
        break;
    case 2u:
        bkgd_size = 6u;
        break;
    case 3u:
    default:
        bkgd_size = 1u;
        break;
    }

    if ((fuzz_cursor_take_u8(cursor, 0u) & 0x03u) == 0u) {
        bkgd_size = 1u + (size_t)(fuzz_cursor_take_u8(cursor, 0u) % sizeof(bkgd));
    }

    while (bkgd_size > 0u) {
        size_t i;

        for (i = 0u; i < bkgd_size; ++i) {
            bkgd[i] = fuzz_cursor_take_u8(cursor, (unsigned char)(i & 0xffu));
        }
        break;
    }

    return fuzz_append_png_chunk(payload,
                                 "bKGD",
                                 bkgd,
                                 bkgd_size,
                                 (int)(fuzz_cursor_take_u8(cursor, 0u) & 0x01u),
                                 (uint32_t)fuzz_cursor_take_u32be(cursor, 1u));
}

static int
fuzz_append_idat_chunk(fuzz_cursor_t *cursor, fuzz_byte_buffer_t *payload)
{
    static unsigned char const fallback_idat[8] = {
        0x78u, 0x9cu, 0x63u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u
    };

    unsigned char const *idat_data;
    size_t idat_size;

    if (cursor == NULL || payload == NULL) {
        return 0;
    }

    idat_data = fallback_idat;
    idat_size = sizeof(fallback_idat);
    if (fuzz_cursor_remaining(cursor) > 0u) {
        idat_size = (size_t)(fuzz_cursor_take_u16be(cursor, 0u) % 1025u);
        if (idat_size > fuzz_cursor_remaining(cursor)) {
            idat_size = fuzz_cursor_remaining(cursor);
        }
        if (idat_size > 0u) {
            idat_data = cursor->data + cursor->pos;
            cursor->pos += idat_size;
        } else {
            idat_data = fallback_idat;
            idat_size = sizeof(fallback_idat);
        }
    }

    return fuzz_append_png_chunk(payload,
                                 "IDAT",
                                 idat_data,
                                 idat_size,
                                 (int)(fuzz_cursor_take_u8(cursor, 0u) & 0x01u),
                                 (uint32_t)fuzz_cursor_take_u32be(cursor, 1u));
}

static int
fuzz_build_png_plte_trns_payload(uint8_t const *data,
                                 size_t size,
                                 fuzz_byte_buffer_t *payload)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    static unsigned char const color_types[] = {3u, 0u, 2u};

    fuzz_cursor_t cursor;
    unsigned char ihdr[13];
    uint32_t width;
    uint32_t height;
    unsigned char color_type;
    unsigned char bit_depth;
    size_t palette_entries;
    unsigned char scenario;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    if (!fuzz_byte_buffer_append(payload, png_signature, sizeof(png_signature))) {
        return 0;
    }

    width = 1u + (uint32_t)(fuzz_cursor_take_u16be(&cursor, 1u) % 1024u);
    height = 1u + (uint32_t)(fuzz_cursor_take_u16be(&cursor, 1u) % 1024u);
    color_type = color_types[fuzz_cursor_take_u8(&cursor, 0u) %
                             (sizeof(color_types) / sizeof(color_types[0]))];
    if (color_type == 3u) {
        bit_depth = fuzz_pick_png_plte_bit_depth(fuzz_cursor_take_u8(&cursor, 0u));
    } else {
        bit_depth = (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u ? 16u : 8u;
    }

    ihdr[0] = (unsigned char)((width >> 24) & 0xffu);
    ihdr[1] = (unsigned char)((width >> 16) & 0xffu);
    ihdr[2] = (unsigned char)((width >> 8) & 0xffu);
    ihdr[3] = (unsigned char)(width & 0xffu);
    ihdr[4] = (unsigned char)((height >> 24) & 0xffu);
    ihdr[5] = (unsigned char)((height >> 16) & 0xffu);
    ihdr[6] = (unsigned char)((height >> 8) & 0xffu);
    ihdr[7] = (unsigned char)(height & 0xffu);
    ihdr[8] = bit_depth;
    ihdr[9] = color_type;
    ihdr[10] = 0u;
    ihdr[11] = 0u;
    ihdr[12] = 0u;

    if (!fuzz_append_png_chunk(payload,
                               "IHDR",
                               ihdr,
                               sizeof(ihdr),
                               (int)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u),
                               (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u))) {
        return 0;
    }

    palette_entries = 0u;
    scenario = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 6u);
    switch (scenario) {
    case 0u:
        if (!fuzz_append_plte_chunk(&cursor, payload, bit_depth, &palette_entries) ||
            !fuzz_append_trns_chunk(&cursor, payload, color_type, palette_entries)) {
            return 0;
        }
        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u &&
            !fuzz_append_bkgd_chunk(&cursor, payload, color_type)) {
            return 0;
        }
        if (color_type == 3u &&
            (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u &&
            !fuzz_append_hist_chunk(&cursor, payload, palette_entries)) {
            return 0;
        }
        break;
    case 1u:
        if (!fuzz_append_trns_chunk(&cursor, payload, color_type, palette_entries) ||
            !fuzz_append_plte_chunk(&cursor, payload, bit_depth, &palette_entries)) {
            return 0;
        }
        break;
    case 2u:
        if (!fuzz_append_plte_chunk(&cursor, payload, bit_depth, &palette_entries)) {
            return 0;
        }
        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u &&
            !fuzz_append_hist_chunk(&cursor, payload, palette_entries)) {
            return 0;
        }
        if (!fuzz_append_trns_chunk(&cursor, payload, color_type, palette_entries)) {
            return 0;
        }
        break;
    case 3u:
        if (!fuzz_append_plte_chunk(&cursor, payload, bit_depth, &palette_entries)) {
            return 0;
        }
        break;
    case 4u:
        if (!fuzz_append_plte_chunk(&cursor, payload, bit_depth, &palette_entries) ||
            !fuzz_append_trns_chunk(&cursor, payload, color_type, palette_entries) ||
            !fuzz_append_plte_chunk(&cursor, payload, bit_depth, &palette_entries)) {
            return 0;
        }
        break;
    default:
        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u || color_type == 3u) {
            if (!fuzz_append_plte_chunk(&cursor, payload, bit_depth, &palette_entries)) {
                return 0;
            }
        }
        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
            if (!fuzz_append_trns_chunk(&cursor, payload, color_type, palette_entries)) {
                return 0;
            }
        }
        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
            if (!fuzz_append_bkgd_chunk(&cursor, payload, color_type)) {
                return 0;
            }
        }
        break;
    }

    if (!fuzz_append_idat_chunk(&cursor, payload) ||
        !fuzz_append_png_chunk(payload, "IEND", NULL, 0u, 0, 0u)) {
        return 0;
    }

    return 1;
}

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    (void)fuzz_loader_builtin_runtime_bootstrap();
    return 0;
}

int
LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
    enum { FUZZ_MAX_INPUT_BYTES = 1 * 1024 * 1024 };

    fuzz_byte_buffer_t payload;

    if (data == NULL || size > (size_t)FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }
    if (!fuzz_loader_builtin_runtime_bootstrap()) {
        return 0;
    }

    fuzz_byte_buffer_init(&payload);
    if (!fuzz_build_png_plte_trns_payload(data, size, &payload)) {
        fuzz_byte_buffer_reset(&payload);
        return 0;
    }

    (void)fuzz_loader_builtin_runtime_run(data,
                                          size,
                                          payload.data,
                                          payload.size);
    fuzz_byte_buffer_reset(&payload);
    return 0;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
