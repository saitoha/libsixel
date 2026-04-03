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
fuzz_pick_png_bit_depth(unsigned char color_type, unsigned char selector)
{
    switch (color_type) {
    case 0u: {
        static unsigned char const depths[] = {1u, 2u, 4u, 8u, 16u};
        return depths[selector % (sizeof(depths) / sizeof(depths[0]))];
    }
    case 2u:
        return (selector & 0x01u) != 0u ? 16u : 8u;
    case 3u: {
        static unsigned char const depths[] = {1u, 2u, 4u, 8u};
        return depths[selector % (sizeof(depths) / sizeof(depths[0]))];
    }
    default:
        return (selector & 0x01u) != 0u ? 16u : 8u;
    }
}

static int
fuzz_append_optional_plte(fuzz_cursor_t *cursor,
                          fuzz_byte_buffer_t *payload,
                          unsigned char bit_depth)
{
    fuzz_byte_buffer_t plte;
    size_t max_entries;
    size_t entries;
    size_t i;

    if (cursor == NULL || payload == NULL) {
        return 0;
    }

    max_entries = (size_t)1u << bit_depth;
    if (max_entries > 256u) {
        max_entries = 256u;
    }
    if (max_entries == 0u) {
        max_entries = 1u;
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
    return 1;
}

static int
fuzz_append_iccp_chunk(fuzz_cursor_t *cursor, fuzz_byte_buffer_t *payload)
{
    static unsigned char const fallback_zlib[16] = {
        0x78u, 0x9cu, 0x63u, 0x60u, 0x60u, 0x60u, 0x64u, 0x00u,
        0x00u, 0x00u, 0x06u, 0x00u, 0x02u, 0x6du, 0x00u, 0x2du
    };

    fuzz_byte_buffer_t iccp;
    unsigned char profile_name[32];
    size_t profile_name_len;
    unsigned char compression_method;
    unsigned char const *profile_blob;
    size_t profile_blob_size;
    size_t i;

    if (cursor == NULL || payload == NULL) {
        return 0;
    }

    profile_name_len = 1u + (size_t)(fuzz_cursor_take_u8(cursor, 0u) % 24u);
    for (i = 0u; i < profile_name_len; ++i) {
        unsigned char ch;

        ch = fuzz_cursor_take_u8(cursor, (unsigned char)('A' + (i % 26u)));
        if (ch == 0u || ch == ' ') {
            ch = (unsigned char)('a' + (i % 26u));
        }
        profile_name[i] = ch;
    }

    compression_method = (fuzz_cursor_take_u8(cursor, 0u) & 0x07u) == 0u
        ? fuzz_cursor_take_u8(cursor, 1u)
        : 0u;

    if (fuzz_cursor_remaining(cursor) > 0u) {
        profile_blob_size = (size_t)(fuzz_cursor_take_u16be(cursor, 0u) % 1025u);
        if (profile_blob_size > fuzz_cursor_remaining(cursor)) {
            profile_blob_size = fuzz_cursor_remaining(cursor);
        }
        if (profile_blob_size > 0u) {
            profile_blob = cursor->data + cursor->pos;
            cursor->pos += profile_blob_size;
        } else {
            profile_blob = fallback_zlib;
            profile_blob_size = sizeof(fallback_zlib);
        }
    } else {
        profile_blob = fallback_zlib;
        profile_blob_size = sizeof(fallback_zlib);
    }

    fuzz_byte_buffer_init(&iccp);
    if (!fuzz_byte_buffer_append(&iccp, profile_name, profile_name_len) ||
        !fuzz_byte_buffer_append_u8(&iccp, 0u) ||
        !fuzz_byte_buffer_append_u8(&iccp, compression_method) ||
        !fuzz_byte_buffer_append(&iccp, profile_blob, profile_blob_size) ||
        !fuzz_append_png_chunk(payload,
                               "iCCP",
                               iccp.data,
                               iccp.size,
                               (int)(fuzz_cursor_take_u8(cursor, 0u) & 0x01u),
                               (uint32_t)fuzz_cursor_take_u32be(cursor, 1u))) {
        fuzz_byte_buffer_reset(&iccp);
        return 0;
    }

    fuzz_byte_buffer_reset(&iccp);
    return 1;
}

static int
fuzz_append_srgb_chunk(fuzz_cursor_t *cursor, fuzz_byte_buffer_t *payload)
{
    unsigned char intent;

    if (cursor == NULL || payload == NULL) {
        return 0;
    }

    intent = (unsigned char)(fuzz_cursor_take_u8(cursor, 0u) % 4u);
    return fuzz_append_png_chunk(payload,
                                 "sRGB",
                                 &intent,
                                 1u,
                                 (int)(fuzz_cursor_take_u8(cursor, 0u) & 0x01u),
                                 (uint32_t)fuzz_cursor_take_u32be(cursor, 1u));
}

static int
fuzz_append_gamma_chunk(fuzz_cursor_t *cursor, fuzz_byte_buffer_t *payload)
{
    unsigned char gamma_bytes[4];
    uint32_t gamma_value;

    if (cursor == NULL || payload == NULL) {
        return 0;
    }

    gamma_value = fuzz_cursor_take_u32be(cursor, 45455u);
    if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) == 0u) {
        gamma_value = 45455u + (gamma_value % 10000u);
    }

    gamma_bytes[0] = (unsigned char)((gamma_value >> 24) & 0xffu);
    gamma_bytes[1] = (unsigned char)((gamma_value >> 16) & 0xffu);
    gamma_bytes[2] = (unsigned char)((gamma_value >> 8) & 0xffu);
    gamma_bytes[3] = (unsigned char)(gamma_value & 0xffu);

    return fuzz_append_png_chunk(payload,
                                 "gAMA",
                                 gamma_bytes,
                                 sizeof(gamma_bytes),
                                 (int)(fuzz_cursor_take_u8(cursor, 0u) & 0x01u),
                                 (uint32_t)fuzz_cursor_take_u32be(cursor, 1u));
}

static int
fuzz_append_chrm_chunk(fuzz_cursor_t *cursor, fuzz_byte_buffer_t *payload)
{
    unsigned char chrm[32];
    size_t i;

    if (cursor == NULL || payload == NULL) {
        return 0;
    }

    for (i = 0u; i < sizeof(chrm); i += 4u) {
        uint32_t v;

        v = fuzz_cursor_take_u32be(cursor, 0u);
        if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) == 0u) {
            v %= 120000u;
        }

        chrm[i + 0u] = (unsigned char)((v >> 24) & 0xffu);
        chrm[i + 1u] = (unsigned char)((v >> 16) & 0xffu);
        chrm[i + 2u] = (unsigned char)((v >> 8) & 0xffu);
        chrm[i + 3u] = (unsigned char)(v & 0xffu);
    }

    return fuzz_append_png_chunk(payload,
                                 "cHRM",
                                 chrm,
                                 sizeof(chrm),
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
        idat_size = (size_t)(fuzz_cursor_take_u16be(cursor, 0u) % 513u);
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
fuzz_build_png_icc_gamma_payload(uint8_t const *data,
                                 size_t size,
                                 fuzz_byte_buffer_t *payload)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    static unsigned char const color_types[] = {0u, 2u, 3u, 6u};

    fuzz_cursor_t cursor;
    unsigned char ihdr[13];
    uint32_t width;
    uint32_t height;
    unsigned char color_type;
    unsigned char bit_depth;
    unsigned char order_mode;

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
    bit_depth = fuzz_pick_png_bit_depth(color_type,
                                        fuzz_cursor_take_u8(&cursor, 0u));

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
    ihdr[12] = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u);

    if (!fuzz_append_png_chunk(payload,
                               "IHDR",
                               ihdr,
                               sizeof(ihdr),
                               (int)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u),
                               (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u))) {
        return 0;
    }

    if (color_type == 3u ||
        (fuzz_cursor_take_u8(&cursor, 0u) & 0x03u) == 0u) {
        if (!fuzz_append_optional_plte(&cursor, payload, bit_depth)) {
            return 0;
        }
    }

    order_mode = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 6u);
    switch (order_mode) {
    case 0u:
        if (!fuzz_append_iccp_chunk(&cursor, payload) ||
            !fuzz_append_gamma_chunk(&cursor, payload) ||
            !fuzz_append_chrm_chunk(&cursor, payload) ||
            !fuzz_append_srgb_chunk(&cursor, payload)) {
            return 0;
        }
        break;
    case 1u:
        if (!fuzz_append_srgb_chunk(&cursor, payload) ||
            !fuzz_append_gamma_chunk(&cursor, payload) ||
            !fuzz_append_iccp_chunk(&cursor, payload)) {
            return 0;
        }
        break;
    case 2u:
        if (!fuzz_append_gamma_chunk(&cursor, payload) ||
            !fuzz_append_chrm_chunk(&cursor, payload)) {
            return 0;
        }
        break;
    case 3u:
        if (!fuzz_append_iccp_chunk(&cursor, payload) ||
            !fuzz_append_srgb_chunk(&cursor, payload)) {
            return 0;
        }
        break;
    case 4u:
        if (!fuzz_append_chrm_chunk(&cursor, payload) ||
            !fuzz_append_iccp_chunk(&cursor, payload) ||
            !fuzz_append_gamma_chunk(&cursor, payload)) {
            return 0;
        }
        break;
    default:
        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u &&
            !fuzz_append_iccp_chunk(&cursor, payload)) {
            return 0;
        }
        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u &&
            !fuzz_append_srgb_chunk(&cursor, payload)) {
            return 0;
        }
        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u &&
            !fuzz_append_gamma_chunk(&cursor, payload)) {
            return 0;
        }
        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u &&
            !fuzz_append_chrm_chunk(&cursor, payload)) {
            return 0;
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
    if (!fuzz_build_png_icc_gamma_payload(data, size, &payload)) {
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
