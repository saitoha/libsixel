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
fuzz_build_png_chunk_payload(uint8_t const *data,
                             size_t size,
                             fuzz_byte_buffer_t *payload)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    static char const chunk_types[][4] = {
        { 'I', 'D', 'A', 'T' },
        { 'P', 'L', 'T', 'E' },
        { 't', 'R', 'N', 'S' },
        { 'g', 'A', 'M', 'A' },
        { 'c', 'H', 'R', 'M' },
        { 's', 'R', 'G', 'B' },
        { 'p', 'H', 'Y', 's' },
        { 't', 'E', 'X', 't' },
        { 'i', 'T', 'X', 't' },
        { 'z', 'T', 'X', 't' },
        { 'i', 'C', 'C', 'P' },
        { 'b', 'K', 'G', 'D' },
        { 's', 'B', 'I', 'T' }
    };

    fuzz_cursor_t cursor;
    unsigned char ihdr[13];
    uint32_t width;
    uint32_t height;
    unsigned char bit_depths[5];
    unsigned char color_types[5];
    size_t chunk_count;
    size_t i;
    int saw_idat;

    if (payload == NULL) {
        return 0;
    }

    bit_depths[0] = 1u;
    bit_depths[1] = 2u;
    bit_depths[2] = 4u;
    bit_depths[3] = 8u;
    bit_depths[4] = 16u;

    color_types[0] = 0u;
    color_types[1] = 2u;
    color_types[2] = 3u;
    color_types[3] = 4u;
    color_types[4] = 6u;

    fuzz_cursor_init(&cursor, data, size);
    if (!fuzz_byte_buffer_append(payload, png_signature, sizeof(png_signature))) {
        return 0;
    }

    width = 1u + (uint32_t)(fuzz_cursor_take_u16be(&cursor, 1u) % 1024u);
    height = 1u + (uint32_t)(fuzz_cursor_take_u16be(&cursor, 1u) % 1024u);

    ihdr[0] = (unsigned char)((width >> 24) & 0xffu);
    ihdr[1] = (unsigned char)((width >> 16) & 0xffu);
    ihdr[2] = (unsigned char)((width >> 8) & 0xffu);
    ihdr[3] = (unsigned char)(width & 0xffu);
    ihdr[4] = (unsigned char)((height >> 24) & 0xffu);
    ihdr[5] = (unsigned char)((height >> 16) & 0xffu);
    ihdr[6] = (unsigned char)((height >> 8) & 0xffu);
    ihdr[7] = (unsigned char)(height & 0xffu);
    ihdr[8] = bit_depths[fuzz_cursor_take_u8(&cursor, 3u) % 5u];
    ihdr[9] = color_types[fuzz_cursor_take_u8(&cursor, 1u) % 5u];
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

    chunk_count = 1u + (size_t)(fuzz_cursor_take_u8(&cursor, 0u) % 6u);
    saw_idat = 0;
    for (i = 0u; i < chunk_count; ++i) {
        char chunk_type[4];
        unsigned char const *chunk_data;
        size_t chunk_length;

        chunk_data = NULL;
        chunk_length = 0u;
        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u &&
            fuzz_cursor_remaining(&cursor) >= 4u) {
            chunk_type[0] = (char)fuzz_cursor_take_u8(&cursor, 'I');
            chunk_type[1] = (char)fuzz_cursor_take_u8(&cursor, 'D');
            chunk_type[2] = (char)fuzz_cursor_take_u8(&cursor, 'A');
            chunk_type[3] = (char)fuzz_cursor_take_u8(&cursor, 'T');
        } else {
            size_t type_index;

            type_index = (size_t)(fuzz_cursor_take_u8(&cursor, 0u) %
                                  (sizeof(chunk_types) / sizeof(chunk_types[0])));
            chunk_type[0] = chunk_types[type_index][0];
            chunk_type[1] = chunk_types[type_index][1];
            chunk_type[2] = chunk_types[type_index][2];
            chunk_type[3] = chunk_types[type_index][3];
        }

        if (chunk_type[0] == 'I' &&
            chunk_type[1] == 'D' &&
            chunk_type[2] == 'A' &&
            chunk_type[3] == 'T') {
            saw_idat = 1;
        }

        if (fuzz_cursor_remaining(&cursor) > 0u) {
            chunk_length = (size_t)(fuzz_cursor_take_u16be(&cursor, 0u) % 1025u);
            if (chunk_length > fuzz_cursor_remaining(&cursor)) {
                chunk_length = fuzz_cursor_remaining(&cursor);
            }
            chunk_data = cursor.data + cursor.pos;
            cursor.pos += chunk_length;
        }

        if (!fuzz_append_png_chunk(payload,
                                   chunk_type,
                                   chunk_data,
                                   chunk_length,
                                   (int)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u),
                                   (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u))) {
            return 0;
        }
    }

    if (!saw_idat) {
        unsigned char const fallback_idat[8] = {
            0x78u, 0x9cu, 0x63u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u
        };
        if (!fuzz_append_png_chunk(payload,
                                   "IDAT",
                                   fallback_idat,
                                   sizeof(fallback_idat),
                                   0,
                                   0u)) {
            return 0;
        }
    }

    if (!fuzz_append_png_chunk(payload, "IEND", NULL, 0u, 0, 0u)) {
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
    if (!fuzz_build_png_chunk_payload(data, size, &payload)) {
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
