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
fuzz_build_bmp_rle_stream(fuzz_cursor_t *cursor,
                          fuzz_byte_buffer_t *stream,
                          int is_rle4)
{
    size_t command_count;
    size_t i;

    if (cursor == NULL || stream == NULL) {
        return 0;
    }

    command_count = 2u + (size_t)(fuzz_cursor_take_u8(cursor, 0u) % 96u);
    for (i = 0u; i < command_count; ++i) {
        unsigned char mode;

        mode = (unsigned char)(fuzz_cursor_take_u8(cursor, 0u) % 6u);
        switch (mode) {
        case 0u: {
            unsigned char run_length;
            unsigned char run_value;

            run_length = (unsigned char)(1u + (fuzz_cursor_take_u8(cursor, 0u) % 255u));
            run_value = fuzz_cursor_take_u8(cursor, 0u);
            if (!fuzz_byte_buffer_append_u8(stream, run_length) ||
                !fuzz_byte_buffer_append_u8(stream, run_value)) {
                return 0;
            }
            break;
        }
        case 1u: {
            unsigned char absolute_pixels;
            size_t data_bytes;
            size_t j;

            absolute_pixels = (unsigned char)(1u + (fuzz_cursor_take_u8(cursor, 0u) % 64u));
            data_bytes = is_rle4 != 0
                ? (size_t)((absolute_pixels + 1u) / 2u)
                : (size_t)absolute_pixels;

            if (!fuzz_byte_buffer_append_u8(stream, 0u) ||
                !fuzz_byte_buffer_append_u8(stream, absolute_pixels)) {
                return 0;
            }

            for (j = 0u; j < data_bytes; ++j) {
                if (!fuzz_byte_buffer_append_u8(stream,
                                                fuzz_cursor_take_u8(cursor,
                                                                    (unsigned char)(j & 0xffu)))) {
                    return 0;
                }
            }

            if ((data_bytes & 0x01u) != 0u) {
                if (!fuzz_byte_buffer_append_u8(stream, 0u)) {
                    return 0;
                }
            }
            break;
        }
        case 2u:
            if (!fuzz_byte_buffer_append_u8(stream, 0u) ||
                !fuzz_byte_buffer_append_u8(stream, 0u)) {
                return 0;
            }
            break;
        case 3u:
            if (!fuzz_byte_buffer_append_u8(stream, 0u) ||
                !fuzz_byte_buffer_append_u8(stream, 2u) ||
                !fuzz_byte_buffer_append_u8(stream,
                                            fuzz_cursor_take_u8(cursor, 0u)) ||
                !fuzz_byte_buffer_append_u8(stream,
                                            fuzz_cursor_take_u8(cursor, 0u))) {
                return 0;
            }
            break;
        case 4u:
            if (!fuzz_byte_buffer_append_u8(stream, 0u) ||
                !fuzz_byte_buffer_append_u8(stream, 1u)) {
                return 0;
            }
            return 1;
        default:
            if (!fuzz_byte_buffer_append_u8(stream, 0u) ||
                !fuzz_byte_buffer_append_u8(stream, 3u) ||
                !fuzz_byte_buffer_append_u8(stream,
                                            fuzz_cursor_take_u8(cursor, 0u)) ||
                !fuzz_byte_buffer_append_u8(stream,
                                            fuzz_cursor_take_u8(cursor, 0u)) ||
                !fuzz_byte_buffer_append_u8(stream,
                                            fuzz_cursor_take_u8(cursor, 0u)) ||
                !fuzz_byte_buffer_append_u8(stream, 0u)) {
                return 0;
            }
            break;
        }
    }

    return fuzz_byte_buffer_append_u8(stream, 0u) &&
           fuzz_byte_buffer_append_u8(stream, 1u);
}

static int
fuzz_append_bmp_masks(fuzz_cursor_t *cursor,
                      fuzz_byte_buffer_t *payload,
                      int include_alpha)
{
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t alpha_mask;

    if (cursor == NULL || payload == NULL) {
        return 0;
    }

    red_mask = fuzz_cursor_take_u32be(cursor, 0x00ff0000u);
    green_mask = fuzz_cursor_take_u32be(cursor, 0x0000ff00u);
    blue_mask = fuzz_cursor_take_u32be(cursor, 0x000000ffu);
    alpha_mask = fuzz_cursor_take_u32be(cursor, 0xff000000u);

    if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) == 0u) {
        red_mask = 0x00ff0000u;
        green_mask = 0x0000ff00u;
        blue_mask = 0x000000ffu;
        alpha_mask = 0xff000000u;
    }

    if (!fuzz_byte_buffer_append_u32le(payload, red_mask) ||
        !fuzz_byte_buffer_append_u32le(payload, green_mask) ||
        !fuzz_byte_buffer_append_u32le(payload, blue_mask)) {
        return 0;
    }

    if (include_alpha != 0) {
        if (!fuzz_byte_buffer_append_u32le(payload, alpha_mask)) {
            return 0;
        }
    }

    return 1;
}

static int
fuzz_build_bmp_rle_bitfields_payload(uint8_t const *data,
                                     size_t size,
                                     fuzz_byte_buffer_t *payload)
{
    fuzz_cursor_t cursor;
    fuzz_byte_buffer_t pixel_data;
    uint32_t dib_size;
    uint32_t width;
    uint32_t height;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t colors_used;
    size_t palette_entries;
    size_t pixel_offset;
    size_t i;
    size_t pixel_data_size;
    unsigned char mode;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    fuzz_byte_buffer_init(&pixel_data);

    mode = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 3u);
    width = 1u + (uint32_t)(fuzz_cursor_take_u8(&cursor, 1u) % 128u);
    height = 1u + (uint32_t)(fuzz_cursor_take_u8(&cursor, 1u) % 128u);
    dib_size = 40u;

    switch (mode) {
    case 0u:
        bits_per_pixel = 8u;
        compression = 1u;
        palette_entries = 1u + (size_t)(fuzz_cursor_take_u8(&cursor, 0u) % 256u);
        break;
    case 1u:
        bits_per_pixel = 4u;
        compression = 2u;
        palette_entries = 1u + (size_t)(fuzz_cursor_take_u8(&cursor, 0u) % 16u);
        break;
    default:
        bits_per_pixel = (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u ? 16u : 32u;
        compression = (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u ? 6u : 3u;
        palette_entries = (fuzz_cursor_take_u8(&cursor, 0u) & 0x07u) == 0u
            ? (size_t)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 8u))
            : 0u;
        break;
    }

    colors_used = (uint32_t)palette_entries;
    pixel_offset = 14u + (size_t)dib_size + palette_entries * 4u;
    if (compression == 3u) {
        pixel_offset += 12u;
    } else if (compression == 6u) {
        pixel_offset += 16u;
    }

    if (!fuzz_byte_buffer_append(payload,
                                 (unsigned char const *)"BM",
                                 2u) ||
        !fuzz_byte_buffer_append_u32le(payload, 0u) ||
        !fuzz_byte_buffer_append_u16le(payload, 0u) ||
        !fuzz_byte_buffer_append_u16le(payload, 0u) ||
        !fuzz_byte_buffer_append_u32le(payload, (uint32_t)pixel_offset) ||
        !fuzz_byte_buffer_append_u32le(payload, dib_size) ||
        !fuzz_byte_buffer_append_u32le(payload, width) ||
        !fuzz_byte_buffer_append_u32le(payload, height) ||
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
        fuzz_byte_buffer_reset(&pixel_data);
        return 0;
    }

    if (compression == 3u || compression == 6u) {
        if (!fuzz_append_bmp_masks(&cursor,
                                   payload,
                                   compression == 6u ? 1 : 0)) {
            fuzz_byte_buffer_reset(&pixel_data);
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
            fuzz_byte_buffer_reset(&pixel_data);
            return 0;
        }
    }

    if (compression == 1u || compression == 2u) {
        if (!fuzz_build_bmp_rle_stream(&cursor,
                                       &pixel_data,
                                       compression == 2u ? 1 : 0)) {
            fuzz_byte_buffer_reset(&pixel_data);
            return 0;
        }
    } else {
        size_t raw_size;

        raw_size = (size_t)(32u + (fuzz_cursor_take_u16be(&cursor, 0u) % 8193u));
        if (raw_size > fuzz_cursor_remaining(&cursor)) {
            raw_size = fuzz_cursor_remaining(&cursor);
        }

        if (raw_size > 0u) {
            if (!fuzz_byte_buffer_append(&pixel_data,
                                         cursor.data + cursor.pos,
                                         raw_size)) {
                fuzz_byte_buffer_reset(&pixel_data);
                return 0;
            }
            cursor.pos += raw_size;
        } else if (!fuzz_byte_buffer_append_zeros(&pixel_data, 64u)) {
            fuzz_byte_buffer_reset(&pixel_data);
            return 0;
        }
    }

    pixel_data_size = pixel_data.size;
    if (!fuzz_byte_buffer_append(payload,
                                 pixel_data.data,
                                 pixel_data.size)) {
        fuzz_byte_buffer_reset(&pixel_data);
        return 0;
    }

    fuzz_byte_buffer_reset(&pixel_data);

    if (payload->size >= 38u) {
        uint32_t file_size;
        uint32_t image_size_field;

        file_size = (uint32_t)payload->size;
        image_size_field = (uint32_t)pixel_data_size;

        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x03u) == 0u) {
            image_size_field ^= (uint32_t)fuzz_cursor_take_u16be(&cursor, 0u);
        }

        payload->data[2] = (unsigned char)(file_size & 0xffu);
        payload->data[3] = (unsigned char)((file_size >> 8) & 0xffu);
        payload->data[4] = (unsigned char)((file_size >> 16) & 0xffu);
        payload->data[5] = (unsigned char)((file_size >> 24) & 0xffu);

        payload->data[34] = (unsigned char)(image_size_field & 0xffu);
        payload->data[35] = (unsigned char)((image_size_field >> 8) & 0xffu);
        payload->data[36] = (unsigned char)((image_size_field >> 16) & 0xffu);
        payload->data[37] = (unsigned char)((image_size_field >> 24) & 0xffu);
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
    if (!fuzz_build_bmp_rle_bitfields_payload(data, size, &payload)) {
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
