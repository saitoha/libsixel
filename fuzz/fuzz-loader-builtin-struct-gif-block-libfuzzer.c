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
fuzz_append_gif_subblocks(fuzz_byte_buffer_t *buffer,
                          fuzz_cursor_t *cursor,
                          size_t max_blocks)
{
    size_t i;

    if (buffer == NULL || cursor == NULL) {
        return 0;
    }

    for (i = 0u; i < max_blocks; ++i) {
        size_t chunk_size;

        if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) == 0u && i > 0u) {
            break;
        }

        chunk_size = (size_t)(1u + (fuzz_cursor_take_u8(cursor, 0u) % 32u));
        if (chunk_size > fuzz_cursor_remaining(cursor)) {
            chunk_size = fuzz_cursor_remaining(cursor);
        }

        if (chunk_size == 0u) {
            break;
        }

        if (!fuzz_byte_buffer_append_u8(buffer, (unsigned char)chunk_size) ||
            !fuzz_byte_buffer_append(buffer,
                                     cursor->data + cursor->pos,
                                     chunk_size)) {
            return 0;
        }
        cursor->pos += chunk_size;
    }

    return fuzz_byte_buffer_append_u8(buffer, 0u);
}

static int
fuzz_build_gif_block_payload(uint8_t const *data,
                             size_t size,
                             fuzz_byte_buffer_t *payload)
{
    fuzz_cursor_t cursor;
    uint16_t width;
    uint16_t height;
    size_t extension_count;
    size_t i;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    width = (uint16_t)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 96u));
    height = (uint16_t)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 96u));

    if (!fuzz_byte_buffer_append(payload,
                                 (unsigned char const *)"GIF89a",
                                 6u) ||
        !fuzz_byte_buffer_append_u16le(payload, width) ||
        !fuzz_byte_buffer_append_u16le(payload, height) ||
        !fuzz_byte_buffer_append_u8(payload, 0x80u) ||
        !fuzz_byte_buffer_append_u8(payload, 0u) ||
        !fuzz_byte_buffer_append_u8(payload, 0u) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    fuzz_cursor_take_u8(&cursor, 0u)) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    fuzz_cursor_take_u8(&cursor, 0u)) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    fuzz_cursor_take_u8(&cursor, 0u)) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    fuzz_cursor_take_u8(&cursor, 0u)) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    fuzz_cursor_take_u8(&cursor, 0u)) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    fuzz_cursor_take_u8(&cursor, 0u))) {
        return 0;
    }

    extension_count = (size_t)(fuzz_cursor_take_u8(&cursor, 0u) % 4u);
    for (i = 0u; i < extension_count; ++i) {
        unsigned char ext_kind;

        ext_kind = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 4u);
        if (!fuzz_byte_buffer_append_u8(payload, 0x21u)) {
            return 0;
        }

        switch (ext_kind) {
        case 0u: {
            if (!fuzz_byte_buffer_append_u8(payload, 0xf9u) ||
                !fuzz_byte_buffer_append_u8(payload, 4u) ||
                !fuzz_byte_buffer_append_u8(payload,
                                            (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) & 0x1fu)) ||
                !fuzz_byte_buffer_append_u16le(payload,
                                               (uint16_t)fuzz_cursor_take_u16be(&cursor, 0u)) ||
                !fuzz_byte_buffer_append_u8(payload,
                                            fuzz_cursor_take_u8(&cursor, 0u)) ||
                !fuzz_byte_buffer_append_u8(payload, 0u)) {
                return 0;
            }
            break;
        }
        case 1u:
            if (!fuzz_byte_buffer_append_u8(payload, 0xfeu) ||
                !fuzz_append_gif_subblocks(payload, &cursor, 4u)) {
                return 0;
            }
            break;
        case 2u:
            if (!fuzz_byte_buffer_append_u8(payload, 0xffu) ||
                !fuzz_byte_buffer_append_u8(payload, 11u) ||
                !fuzz_byte_buffer_append(payload,
                                         (unsigned char const *)"NETSCAPE2.0",
                                         11u) ||
                !fuzz_append_gif_subblocks(payload, &cursor, 3u)) {
                return 0;
            }
            break;
        default:
            if (!fuzz_byte_buffer_append_u8(payload, 0x01u) ||
                !fuzz_byte_buffer_append_u8(payload, 12u) ||
                !fuzz_byte_buffer_append_zeros(payload, 12u) ||
                !fuzz_append_gif_subblocks(payload, &cursor, 3u)) {
                return 0;
            }
            break;
        }
    }

    if (!fuzz_byte_buffer_append_u8(payload, 0x2cu) ||
        !fuzz_byte_buffer_append_u16le(payload,
                                       (uint16_t)(fuzz_cursor_take_u8(&cursor, 0u) % 4u)) ||
        !fuzz_byte_buffer_append_u16le(payload,
                                       (uint16_t)(fuzz_cursor_take_u8(&cursor, 0u) % 4u)) ||
        !fuzz_byte_buffer_append_u16le(payload, width) ||
        !fuzz_byte_buffer_append_u16le(payload, height) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) & 0xc0u)) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    (unsigned char)(2u + (fuzz_cursor_take_u8(&cursor, 0u) % 6u))) ||
        !fuzz_append_gif_subblocks(payload, &cursor, 6u) ||
        !fuzz_byte_buffer_append_u8(payload, 0x3bu)) {
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
    if (!fuzz_build_gif_block_payload(data, size, &payload)) {
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
