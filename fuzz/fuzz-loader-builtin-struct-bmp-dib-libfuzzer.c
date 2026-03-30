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

static int
fuzz_build_bmp_dib_payload(uint8_t const *data,
                           size_t size,
                           fuzz_byte_buffer_t *payload)
{
    static uint16_t const bpp_table[] = {1u, 4u, 8u, 16u, 24u, 32u};

    fuzz_cursor_t cursor;
    uint32_t dib_size;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t colors_used;
    size_t palette_entries;
    size_t i;
    size_t pixel_data_size;
    size_t pixel_offset;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    dib_size = (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u ? 40u : 124u;
    bits_per_pixel = bpp_table[fuzz_cursor_take_u8(&cursor, 0u) %
                               (sizeof(bpp_table) / sizeof(bpp_table[0]))];
    compression = (uint32_t)(fuzz_cursor_take_u8(&cursor, 0u) % 7u);

    palette_entries = 0u;
    if (bits_per_pixel <= 8u) {
        palette_entries = 1u << bits_per_pixel;
        palette_entries = (size_t)(1u +
            (fuzz_cursor_take_u8(&cursor, (unsigned char)palette_entries - 1u) % palette_entries));
    }
    colors_used = (uint32_t)palette_entries;

    pixel_offset = 14u + dib_size + palette_entries * 4u;

    if (!fuzz_byte_buffer_append(payload,
                                 (unsigned char const *)"BM",
                                 2u) ||
        !fuzz_byte_buffer_append_u32le(payload, 0u) ||
        !fuzz_byte_buffer_append_u16le(payload, 0u) ||
        !fuzz_byte_buffer_append_u16le(payload, 0u) ||
        !fuzz_byte_buffer_append_u32le(payload, (uint32_t)pixel_offset) ||
        !fuzz_byte_buffer_append_u32le(payload, dib_size) ||
        !fuzz_byte_buffer_append_u32le(payload,
                                       (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u)) ||
        !fuzz_byte_buffer_append_u32le(payload,
                                       (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u)) ||
        !fuzz_byte_buffer_append_u16le(payload, 1u) ||
        !fuzz_byte_buffer_append_u16le(payload, bits_per_pixel) ||
        !fuzz_byte_buffer_append_u32le(payload, compression) ||
        !fuzz_byte_buffer_append_u32le(payload,
                                       (uint32_t)fuzz_cursor_take_u32be(&cursor, 0u)) ||
        !fuzz_byte_buffer_append_u32le(payload,
                                       (uint32_t)fuzz_cursor_take_u32be(&cursor, 2835u)) ||
        !fuzz_byte_buffer_append_u32le(payload,
                                       (uint32_t)fuzz_cursor_take_u32be(&cursor, 2835u)) ||
        !fuzz_byte_buffer_append_u32le(payload, colors_used) ||
        !fuzz_byte_buffer_append_u32le(payload,
                                       (uint32_t)fuzz_cursor_take_u32be(&cursor, 0u))) {
        return 0;
    }

    if (dib_size > 40u) {
        if (!fuzz_byte_buffer_append_zeros(payload, dib_size - 40u)) {
            return 0;
        }
    }

    for (i = 0u; i < palette_entries; ++i) {
        if (!fuzz_byte_buffer_append_u8(payload,
                                        fuzz_cursor_take_u8(&cursor, 0u)) ||
            !fuzz_byte_buffer_append_u8(payload,
                                        fuzz_cursor_take_u8(&cursor, 0u)) ||
            !fuzz_byte_buffer_append_u8(payload,
                                        fuzz_cursor_take_u8(&cursor, 0u)) ||
            !fuzz_byte_buffer_append_u8(payload, 0u)) {
            return 0;
        }
    }

    pixel_data_size = fuzz_cursor_remaining(&cursor);
    if (pixel_data_size > 0u) {
        if (!fuzz_byte_buffer_append(payload,
                                     cursor.data + cursor.pos,
                                     pixel_data_size)) {
            return 0;
        }
    } else if (!fuzz_byte_buffer_append_zeros(payload, 32u)) {
        return 0;
    }

    if (payload->size >= 6u) {
        uint32_t file_size;

        file_size = (uint32_t)payload->size;
        payload->data[2] = (unsigned char)(file_size & 0xffu);
        payload->data[3] = (unsigned char)((file_size >> 8) & 0xffu);
        payload->data[4] = (unsigned char)((file_size >> 16) & 0xffu);
        payload->data[5] = (unsigned char)((file_size >> 24) & 0xffu);
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
    if (!fuzz_build_bmp_dib_payload(data, size, &payload)) {
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
