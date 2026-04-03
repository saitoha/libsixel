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
fuzz_gif_pack_code(fuzz_byte_buffer_t *stream,
                   uint32_t *bit_acc,
                   unsigned int *bit_count,
                   uint16_t code,
                   unsigned int width)
{
    if (stream == NULL || bit_acc == NULL || bit_count == NULL) {
        return 0;
    }
    if (width == 0u || width > 12u) {
        return 0;
    }

    *bit_acc |= ((uint32_t)code << *bit_count);
    *bit_count += width;

    while (*bit_count >= 8u) {
        unsigned char out;

        out = (unsigned char)(*bit_acc & 0xffu);
        if (!fuzz_byte_buffer_append_u8(stream, out)) {
            return 0;
        }
        *bit_acc >>= 8u;
        *bit_count -= 8u;
    }

    return 1;
}

static int
fuzz_gif_flush_codes(fuzz_byte_buffer_t *stream,
                     uint32_t *bit_acc,
                     unsigned int *bit_count)
{
    if (stream == NULL || bit_acc == NULL || bit_count == NULL) {
        return 0;
    }

    if (*bit_count > 0u) {
        if (!fuzz_byte_buffer_append_u8(stream, (unsigned char)(*bit_acc & 0xffu))) {
            return 0;
        }
        *bit_acc = 0u;
        *bit_count = 0u;
    }

    return 1;
}

static int
fuzz_build_gif_lzw_stream(fuzz_cursor_t *cursor,
                          unsigned int min_code_size,
                          fuzz_byte_buffer_t *stream)
{
    uint16_t clear_code;
    uint16_t eoi_code;
    uint16_t next_code;
    unsigned int code_size;
    unsigned int max_code_value;
    uint32_t bit_acc;
    unsigned int bit_count;
    int saw_eoi;
    int prev_was_clear;
    size_t i;
    size_t op_count;

    if (cursor == NULL || stream == NULL || min_code_size < 2u || min_code_size > 8u) {
        return 0;
    }

    clear_code = (uint16_t)(1u << min_code_size);
    eoi_code = (uint16_t)(clear_code + 1u);
    next_code = (uint16_t)(eoi_code + 1u);
    code_size = min_code_size + 1u;
    max_code_value = 1u << code_size;
    bit_acc = 0u;
    bit_count = 0u;
    saw_eoi = 0;
    prev_was_clear = 1;

    if (!fuzz_gif_pack_code(stream,
                            &bit_acc,
                            &bit_count,
                            clear_code,
                            code_size)) {
        return 0;
    }

    op_count = 4u + (size_t)(fuzz_cursor_take_u8(cursor, 0u) % 160u);
    for (i = 0u; i < op_count; ++i) {
        unsigned char op;
        uint16_t code;

        op = fuzz_cursor_take_u8(cursor, 0u);

        if (i > 1u && (op & 0x1fu) == 0u) {
            code = clear_code;
        } else if (i > 2u && (op & 0x3fu) == 1u) {
            code = eoi_code;
            saw_eoi = 1;
        } else if (prev_was_clear) {
            code = (uint16_t)(fuzz_cursor_take_u16be(cursor, 0u) % clear_code);
        } else {
            uint16_t upper;

            upper = next_code;
            if (upper < clear_code) {
                upper = clear_code;
            }
            if (upper > 4095u) {
                upper = 4095u;
            }

            if ((op & 0x01u) == 0u) {
                code = (uint16_t)(fuzz_cursor_take_u16be(cursor, 0u) % clear_code);
            } else {
                code = (uint16_t)(fuzz_cursor_take_u16be(cursor, 0u) % (uint16_t)(upper + 1u));
            }
        }

        if (!fuzz_gif_pack_code(stream,
                                &bit_acc,
                                &bit_count,
                                code,
                                code_size)) {
            return 0;
        }

        if (code == clear_code) {
            code_size = min_code_size + 1u;
            max_code_value = 1u << code_size;
            next_code = (uint16_t)(eoi_code + 1u);
            prev_was_clear = 1;
            continue;
        }

        if (code == eoi_code) {
            break;
        }

        if (!prev_was_clear) {
            if (next_code < 4095u) {
                ++next_code;
            }
            if ((unsigned int)next_code >= max_code_value && code_size < 12u) {
                ++code_size;
                max_code_value <<= 1u;
            }
        }
        prev_was_clear = 0;
    }

    if (!saw_eoi) {
        if (!fuzz_gif_pack_code(stream,
                                &bit_acc,
                                &bit_count,
                                eoi_code,
                                code_size)) {
            return 0;
        }
    }

    return fuzz_gif_flush_codes(stream, &bit_acc, &bit_count);
}

static int
fuzz_append_gif_subblocks_from_stream(fuzz_cursor_t *cursor,
                                      fuzz_byte_buffer_t *payload,
                                      fuzz_byte_buffer_t const *stream)
{
    size_t pos;

    if (cursor == NULL || payload == NULL || stream == NULL) {
        return 0;
    }

    pos = 0u;
    while (pos < stream->size) {
        size_t chunk_size;

        chunk_size = 1u + (size_t)(fuzz_cursor_take_u8(cursor, 0u) % 64u);
        if (chunk_size > stream->size - pos) {
            chunk_size = stream->size - pos;
        }

        if (!fuzz_byte_buffer_append_u8(payload, (unsigned char)chunk_size) ||
            !fuzz_byte_buffer_append(payload,
                                     stream->data + pos,
                                     chunk_size)) {
            return 0;
        }
        pos += chunk_size;

        if (pos < stream->size &&
            (fuzz_cursor_take_u8(cursor, 0u) & 0x3fu) == 0u) {
            break;
        }
    }

    return fuzz_byte_buffer_append_u8(payload, 0u);
}

static int
fuzz_build_gif_lzw_codes_payload(uint8_t const *data,
                                 size_t size,
                                 fuzz_byte_buffer_t *payload)
{
    fuzz_cursor_t cursor;
    fuzz_byte_buffer_t stream;
    unsigned int min_code_size;
    uint16_t width;
    uint16_t height;
    unsigned int color_table_entries;
    unsigned char packed;
    unsigned int i;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    fuzz_byte_buffer_init(&stream);

    min_code_size = 2u + (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 4u);
    color_table_entries = 1u << min_code_size;

    width = (uint16_t)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 96u));
    height = (uint16_t)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 96u));
    packed = (unsigned char)(0x80u | 0x70u | ((min_code_size - 1u) & 0x07u));

    if (!fuzz_byte_buffer_append(payload,
                                 (unsigned char const *)"GIF89a",
                                 6u) ||
        !fuzz_byte_buffer_append_u16le(payload, width) ||
        !fuzz_byte_buffer_append_u16le(payload, height) ||
        !fuzz_byte_buffer_append_u8(payload, packed) ||
        !fuzz_byte_buffer_append_u8(payload, 0u) ||
        !fuzz_byte_buffer_append_u8(payload, 0u)) {
        fuzz_byte_buffer_reset(&stream);
        return 0;
    }

    for (i = 0u; i < color_table_entries; ++i) {
        if (!fuzz_byte_buffer_append_u8(payload,
                                        fuzz_cursor_take_u8(&cursor,
                                                            (unsigned char)(i & 0xffu))) ||
            !fuzz_byte_buffer_append_u8(payload,
                                        fuzz_cursor_take_u8(&cursor,
                                                            (unsigned char)((i * 3u) & 0xffu))) ||
            !fuzz_byte_buffer_append_u8(payload,
                                        fuzz_cursor_take_u8(&cursor,
                                                            (unsigned char)((i * 5u) & 0xffu)))) {
            fuzz_byte_buffer_reset(&stream);
            return 0;
        }
    }

    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
        if (!fuzz_byte_buffer_append_u8(payload, 0x21u) ||
            !fuzz_byte_buffer_append_u8(payload, 0xf9u) ||
            !fuzz_byte_buffer_append_u8(payload, 4u) ||
            !fuzz_byte_buffer_append_u8(payload,
                                        (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) & 0x1fu)) ||
            !fuzz_byte_buffer_append_u16le(payload,
                                           (uint16_t)fuzz_cursor_take_u16be(&cursor, 0u)) ||
            !fuzz_byte_buffer_append_u8(payload,
                                        fuzz_cursor_take_u8(&cursor, 0u)) ||
            !fuzz_byte_buffer_append_u8(payload, 0u)) {
            fuzz_byte_buffer_reset(&stream);
            return 0;
        }
    }

    if (!fuzz_byte_buffer_append_u8(payload, 0x2cu) ||
        !fuzz_byte_buffer_append_u16le(payload,
                                       (uint16_t)(fuzz_cursor_take_u8(&cursor, 0u) % 8u)) ||
        !fuzz_byte_buffer_append_u16le(payload,
                                       (uint16_t)(fuzz_cursor_take_u8(&cursor, 0u) % 8u)) ||
        !fuzz_byte_buffer_append_u16le(payload, width) ||
        !fuzz_byte_buffer_append_u16le(payload, height) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) & 0x40u)) ||
        !fuzz_byte_buffer_append_u8(payload, (unsigned char)min_code_size)) {
        fuzz_byte_buffer_reset(&stream);
        return 0;
    }

    if (!fuzz_build_gif_lzw_stream(&cursor, min_code_size, &stream) ||
        !fuzz_append_gif_subblocks_from_stream(&cursor, payload, &stream) ||
        !fuzz_byte_buffer_append_u8(payload, 0x3bu)) {
        fuzz_byte_buffer_reset(&stream);
        return 0;
    }

    fuzz_byte_buffer_reset(&stream);
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
    if (!fuzz_build_gif_lzw_codes_payload(data, size, &payload)) {
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
