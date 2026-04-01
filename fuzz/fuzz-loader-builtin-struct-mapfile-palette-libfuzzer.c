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
#include <stdio.h>
#include <string.h>

#include <sixel.h>

#include "compat_stub.h"
#include "mapfile.h"
#include "fuzz-loader-builtin-struct-common.h"

static void
fuzz_disable_option_suggestions(void)
{
    (void)sixel_compat_setenv("SIXEL_OPTION_PREFIX_SUGGESTIONS", "0");
    (void)sixel_compat_setenv("SIXEL_OPTION_FUZZY_SUGGESTIONS", "0");
}

static void
fuzz_patch_u32le(unsigned char *ptr, uint32_t value)
{
    if (ptr == NULL) {
        return;
    }
    ptr[0] = (unsigned char)(value & 0xffu);
    ptr[1] = (unsigned char)((value >> 8) & 0xffu);
    ptr[2] = (unsigned char)((value >> 16) & 0xffu);
    ptr[3] = (unsigned char)((value >> 24) & 0xffu);
}

static int
fuzz_append_text(fuzz_byte_buffer_t *buffer, char const *text)
{
    size_t len;

    if (buffer == NULL || text == NULL) {
        return 0;
    }

    len = strlen(text);
    return fuzz_byte_buffer_append(buffer,
                                   (unsigned char const *)(uintptr_t)text,
                                   len);
}

static int
fuzz_append_line3(fuzz_byte_buffer_t *buffer, int x, int y, int z)
{
    char line[80];
    int written;

    if (buffer == NULL) {
        return 0;
    }

    written = snprintf(line, sizeof(line), "%d %d %d\n", x, y, z);
    if (written < 0 || (size_t)written >= sizeof(line)) {
        return 0;
    }

    return fuzz_byte_buffer_append(buffer,
                                   (unsigned char const *)(uintptr_t)line,
                                   (size_t)written);
}

static int
fuzz_append_number_line(fuzz_byte_buffer_t *buffer, long value)
{
    char line[48];
    int written;

    if (buffer == NULL) {
        return 0;
    }

    written = snprintf(line, sizeof(line), "%ld\n", value);
    if (written < 0 || (size_t)written >= sizeof(line)) {
        return 0;
    }

    return fuzz_byte_buffer_append(buffer,
                                   (unsigned char const *)(uintptr_t)line,
                                   (size_t)written);
}

static unsigned char
fuzz_take_fill_byte(fuzz_cursor_t *cursor, size_t index)
{
    return fuzz_cursor_take_u8(cursor,
                               (unsigned char)((index * 37u + 11u) & 0xffu));
}

static int
fuzz_append_fill_bytes(fuzz_byte_buffer_t *buffer,
                       fuzz_cursor_t *cursor,
                       size_t length)
{
    size_t i;

    if (buffer == NULL) {
        return 0;
    }

    for (i = 0u; i < length; ++i) {
        if (!fuzz_byte_buffer_append_u8(buffer,
                                        fuzz_take_fill_byte(cursor, i))) {
            return 0;
        }
    }

    return 1;
}

static int
fuzz_take_component(fuzz_cursor_t *cursor, int wide_range)
{
    int value;

    value = (int)fuzz_cursor_take_u16be(cursor, 0u);
    if (wide_range) {
        return (value % 768) - 256;
    }
    return value % 256;
}

static int
fuzz_build_act_payload(fuzz_cursor_t *cursor, fuzz_byte_buffer_t *payload)
{
    size_t palette_bytes;
    unsigned char mode;

    if (payload == NULL) {
        return 0;
    }

    mode = (unsigned char)(fuzz_cursor_take_u8(cursor, 0u) % 5u);
    palette_bytes = 256u * 3u;

    if (mode == 0u) {
        return fuzz_append_fill_bytes(payload, cursor, palette_bytes);
    }

    if (mode == 1u || mode == 4u) {
        unsigned int exported;
        unsigned int start_index;

        if (!fuzz_append_fill_bytes(payload, cursor, palette_bytes)) {
            return 0;
        }

        exported = (unsigned int)(1u + (fuzz_cursor_take_u8(cursor, 0u) % 255u));
        start_index = (unsigned int)fuzz_cursor_take_u8(cursor, 0u);
        if (mode == 4u) {
            exported = (unsigned int)(250u + (fuzz_cursor_take_u8(cursor, 0u) % 8u));
            start_index = (unsigned int)(252u + (fuzz_cursor_take_u8(cursor, 0u) % 4u));
        }

        return fuzz_byte_buffer_append_u8(payload,
                                          (unsigned char)((exported >> 8) & 0xffu)) &&
               fuzz_byte_buffer_append_u8(payload,
                                          (unsigned char)(exported & 0xffu)) &&
               fuzz_byte_buffer_append_u8(payload,
                                          (unsigned char)((start_index >> 8) & 0xffu)) &&
               fuzz_byte_buffer_append_u8(payload,
                                          (unsigned char)(start_index & 0xffu));
    }

    if (mode == 2u) {
        size_t truncated;

        truncated = (size_t)fuzz_cursor_take_u16be(cursor, 0u);
        if (truncated >= palette_bytes) {
            truncated = palette_bytes - 1u;
        }
        return fuzz_append_fill_bytes(payload, cursor, truncated);
    }

    /* invalid length that is neither 768 nor 772 */
    {
        size_t invalid_size;

        invalid_size = 769u + (size_t)(fuzz_cursor_take_u8(cursor, 0u) % 32u);
        if (invalid_size == palette_bytes + 4u) {
            invalid_size += 1u;
        }
        return fuzz_append_fill_bytes(payload, cursor, invalid_size);
    }
}

static int
fuzz_build_pal_jasc_payload(fuzz_cursor_t *cursor, fuzz_byte_buffer_t *payload)
{
    unsigned char with_bom;
    unsigned char header_mode;
    int declared_count;
    int emitted_count;
    int i;
    int wide_values;

    if (payload == NULL) {
        return 0;
    }

    with_bom = (unsigned char)(fuzz_cursor_take_u8(cursor, 0u) & 0x01u);
    header_mode = (unsigned char)(fuzz_cursor_take_u8(cursor, 0u) % 4u);

    if (with_bom) {
        if (!fuzz_byte_buffer_append_u8(payload, 0xefu) ||
            !fuzz_byte_buffer_append_u8(payload, 0xbbu) ||
            !fuzz_byte_buffer_append_u8(payload, 0xbfu)) {
            return 0;
        }
    }

    switch (header_mode) {
    case 0u:
        if (!fuzz_append_text(payload, "JASC-PAL\n") ||
            !fuzz_append_text(payload, "0100\n")) {
            return 0;
        }
        break;
    case 1u:
        if (!fuzz_append_text(payload, "JASC-PAZ\n") ||
            !fuzz_append_text(payload, "0100\n")) {
            return 0;
        }
        break;
    case 2u:
        if (!fuzz_append_text(payload, "JASC-PAL\n") ||
            !fuzz_append_text(payload, "X100\n")) {
            return 0;
        }
        break;
    default:
        if (!fuzz_append_text(payload, "# missing header path\n")) {
            return 0;
        }
        break;
    }

    declared_count = (int)(fuzz_cursor_take_u16be(cursor, 1u) % 300u);
    if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u) {
        declared_count = 1 + (declared_count % 32);
    }
    if (!fuzz_append_number_line(payload, declared_count)) {
        return 0;
    }

    emitted_count = declared_count;
    emitted_count += (int)((fuzz_cursor_take_u8(cursor, 0u) % 5u) - 2);
    if (emitted_count < 0) {
        emitted_count = 0;
    }
    if (emitted_count > 48) {
        emitted_count = 48;
    }

    wide_values = ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u);
    for (i = 0; i < emitted_count; ++i) {
        if ((fuzz_cursor_take_u8(cursor, 0u) & 0x07u) == 0u) {
            if (!fuzz_append_text(payload, "# fuzz comment\n")) {
                return 0;
            }
        }

        if (!fuzz_append_line3(payload,
                               fuzz_take_component(cursor, wide_values),
                               fuzz_take_component(cursor, wide_values),
                               fuzz_take_component(cursor, wide_values))) {
            return 0;
        }
    }

    return 1;
}

static int
fuzz_build_pal_riff_payload(fuzz_cursor_t *cursor, fuzz_byte_buffer_t *payload)
{
    size_t riff_size_offset;
    unsigned char include_data;
    unsigned char align_data;

    if (payload == NULL) {
        return 0;
    }

    if (!fuzz_append_text(payload, "RIFF")) {
        return 0;
    }
    riff_size_offset = payload->size;
    if (!fuzz_byte_buffer_append_u32le(payload, 0u) ||
        !fuzz_append_text(payload, "PAL ")) {
        return 0;
    }

    include_data = (unsigned char)(fuzz_cursor_take_u8(cursor, 0u) & 0x01u);
    align_data = (unsigned char)(fuzz_cursor_take_u8(cursor, 0u) & 0x01u);

    if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u) {
        size_t junk_size;

        junk_size = (size_t)(fuzz_cursor_take_u8(cursor, 0u) % 16u);
        if (!fuzz_append_text(payload, "JUNK") ||
            !fuzz_byte_buffer_append_u32le(payload, (uint32_t)junk_size) ||
            !fuzz_append_fill_bytes(payload, cursor, junk_size)) {
            return 0;
        }
        if ((junk_size & 1u) != 0u &&
            !fuzz_byte_buffer_append_u8(payload, 0u)) {
            return 0;
        }
    }

    if (include_data) {
        size_t data_size_offset;
        size_t data_payload_begin;
        unsigned int declared_entries;
        unsigned int actual_entries;
        unsigned int i;
        uint32_t declared_chunk_size;

        if (!fuzz_append_text(payload, "data")) {
            return 0;
        }
        data_size_offset = payload->size;
        if (!fuzz_byte_buffer_append_u32le(payload, 0u)) {
            return 0;
        }

        data_payload_begin = payload->size;
        declared_entries = (unsigned int)(fuzz_cursor_take_u16be(cursor, 1u) % 320u);
        actual_entries = (unsigned int)(fuzz_cursor_take_u8(cursor, 0u) % 40u);

        if (!fuzz_byte_buffer_append_u16le(payload,
                                           fuzz_cursor_take_u16be(cursor,
                                                                  0x0300u)) ||
            !fuzz_byte_buffer_append_u16le(payload,
                                           (uint16_t)declared_entries)) {
            return 0;
        }

        for (i = 0u; i < actual_entries; ++i) {
            if (!fuzz_byte_buffer_append_u8(payload,
                                            fuzz_cursor_take_u8(cursor,
                                                                (unsigned char)i)) ||
                !fuzz_byte_buffer_append_u8(payload,
                                            fuzz_cursor_take_u8(cursor,
                                                                (unsigned char)(i + 1u))) ||
                !fuzz_byte_buffer_append_u8(payload,
                                            fuzz_cursor_take_u8(cursor,
                                                                (unsigned char)(i + 2u))) ||
                !fuzz_byte_buffer_append_u8(payload,
                                            fuzz_cursor_take_u8(cursor, 0u))) {
                return 0;
            }
        }

        declared_chunk_size = (uint32_t)(payload->size - data_payload_begin);
        if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u) {
            declared_chunk_size += (uint32_t)(fuzz_cursor_take_u8(cursor, 0u) % 9u);
        }

        fuzz_patch_u32le(payload->data + data_size_offset, declared_chunk_size);
        if (align_data && ((declared_chunk_size & 1u) != 0u) &&
            !fuzz_byte_buffer_append_u8(payload, 0u)) {
            return 0;
        }
    } else {
        size_t list_size;

        list_size = (size_t)(fuzz_cursor_take_u8(cursor, 0u) % 16u);
        if (!fuzz_append_text(payload, "LIST") ||
            !fuzz_byte_buffer_append_u32le(payload, (uint32_t)list_size) ||
            !fuzz_append_fill_bytes(payload, cursor, list_size)) {
            return 0;
        }
        if ((list_size & 1u) != 0u &&
            !fuzz_byte_buffer_append_u8(payload, 0u)) {
            return 0;
        }
    }

    if (payload->size >= 8u) {
        uint32_t riff_size;

        riff_size = (uint32_t)(payload->size - 8u);
        if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u) {
            riff_size ^= (uint32_t)(fuzz_cursor_take_u8(cursor, 0u) << 8);
        }
        fuzz_patch_u32le(payload->data + riff_size_offset, riff_size);
    }

    return 1;
}

static int
fuzz_build_gpl_payload(fuzz_cursor_t *cursor, fuzz_byte_buffer_t *payload)
{
    int header_ok;
    int include_meta;
    int color_count;
    int i;
    int wide_values;

    if (payload == NULL) {
        return 0;
    }

    if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u) {
        if (!fuzz_byte_buffer_append_u8(payload, 0xefu) ||
            !fuzz_byte_buffer_append_u8(payload, 0xbbu) ||
            !fuzz_byte_buffer_append_u8(payload, 0xbfu)) {
            return 0;
        }
    }

    header_ok = ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u);
    include_meta = ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u);

    if (header_ok) {
        if (!fuzz_append_text(payload, "GIMP Palette\n")) {
            return 0;
        }
    } else {
        if (!fuzz_append_text(payload, "GIMP PalXtte\n")) {
            return 0;
        }
    }

    if (include_meta) {
        if (!fuzz_append_text(payload, "Name: fuzz-mapfile\n") ||
            !fuzz_append_text(payload, "Columns: 8\n") ||
            !fuzz_append_text(payload, "# generated by libFuzzer\n")) {
            return 0;
        }
    }

    color_count = (int)(fuzz_cursor_take_u8(cursor, 0u) % 80u);
    if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u) {
        color_count = 1 + (color_count % 32);
    }

    wide_values = ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u);
    for (i = 0; i < color_count; ++i) {
        if ((fuzz_cursor_take_u8(cursor, 0u) & 0x0fu) == 0u) {
            if (!fuzz_append_text(payload, "# comment\n")) {
                return 0;
            }
        }

        if (!fuzz_append_line3(payload,
                               fuzz_take_component(cursor, wide_values),
                               fuzz_take_component(cursor, wide_values),
                               fuzz_take_component(cursor, wide_values))) {
            return 0;
        }
    }

    return 1;
}

static void
fuzz_try_parse_format(sixel_palette_format_t format,
                      unsigned char const *data,
                      size_t size,
                      sixel_encoder_t *encoder)
{
    sixel_dither_t *dither;

    if (encoder == NULL) {
        return;
    }

    dither = NULL;

    switch (format) {
    case SIXEL_PALETTE_FORMAT_ACT:
        (void)sixel_palette_parse_act(data, size, encoder, &dither);
        break;
    case SIXEL_PALETTE_FORMAT_PAL_JASC:
        (void)sixel_palette_parse_pal_jasc(data, size, encoder, &dither);
        break;
    case SIXEL_PALETTE_FORMAT_PAL_RIFF:
        (void)sixel_palette_parse_pal_riff(data, size, encoder, &dither);
        break;
    case SIXEL_PALETTE_FORMAT_GPL:
        (void)sixel_palette_parse_gpl(data, size, encoder, &dither);
        break;
    default:
        break;
    }

    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
}

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    fuzz_disable_option_suggestions();
    return 0;
}

int
LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
    enum { FUZZ_MAX_INPUT_BYTES = 256 * 1024 };

    fuzz_cursor_t cursor;
    fuzz_byte_buffer_t payload;
    sixel_encoder_t *encoder;
    sixel_palette_format_t selected_format;
    sixel_palette_format_t guessed_format;
    unsigned char mode;

    if (data == NULL || size > (size_t)FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    fuzz_byte_buffer_init(&payload);
    encoder = NULL;
    selected_format = SIXEL_PALETTE_FORMAT_NONE;

    mode = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 4u);
    switch (mode) {
    case 0u:
        selected_format = SIXEL_PALETTE_FORMAT_ACT;
        if (!fuzz_build_act_payload(&cursor, &payload)) {
            goto end;
        }
        break;
    case 1u:
        selected_format = SIXEL_PALETTE_FORMAT_PAL_JASC;
        if (!fuzz_build_pal_jasc_payload(&cursor, &payload)) {
            goto end;
        }
        break;
    case 2u:
        selected_format = SIXEL_PALETTE_FORMAT_PAL_RIFF;
        if (!fuzz_build_pal_riff_payload(&cursor, &payload)) {
            goto end;
        }
        break;
    case 3u:
    default:
        selected_format = SIXEL_PALETTE_FORMAT_GPL;
        if (!fuzz_build_gpl_payload(&cursor, &payload)) {
            goto end;
        }
        break;
    }

    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) || encoder == NULL) {
        goto end;
    }

    guessed_format = sixel_palette_guess_format(payload.data, payload.size);

    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u &&
        guessed_format != SIXEL_PALETTE_FORMAT_NONE) {
        fuzz_try_parse_format(guessed_format,
                              payload.data,
                              payload.size,
                              encoder);
    }

    fuzz_try_parse_format(selected_format,
                          payload.data,
                          payload.size,
                          encoder);

    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
        fuzz_try_parse_format(SIXEL_PALETTE_FORMAT_ACT,
                              payload.data,
                              payload.size,
                              encoder);
        fuzz_try_parse_format(SIXEL_PALETTE_FORMAT_PAL_JASC,
                              payload.data,
                              payload.size,
                              encoder);
        fuzz_try_parse_format(SIXEL_PALETTE_FORMAT_PAL_RIFF,
                              payload.data,
                              payload.size,
                              encoder);
        fuzz_try_parse_format(SIXEL_PALETTE_FORMAT_GPL,
                              payload.data,
                              payload.size,
                              encoder);
    }

end:
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
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
