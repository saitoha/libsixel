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

#include "fuzz-loader-builtin-struct-common.h"

static int
fuzz_append_cstr(fuzz_byte_buffer_t *buffer, char const *text)
{
    size_t len;

    if (buffer == NULL || text == NULL) {
        return 0;
    }
    len = 0u;
    while (text[len] != '\0') {
        ++len;
    }
    return fuzz_byte_buffer_append(buffer,
                                   (unsigned char const *)(uintptr_t)text,
                                   len);
}

static int
fuzz_append_decimal_token(fuzz_byte_buffer_t *buffer,
                          uint32_t value,
                          unsigned char extra_digits)
{
    char temp[48];
    int n;
    unsigned char i;

    n = snprintf(temp, sizeof(temp), "%u", value);
    if (n < 0 || (size_t)n >= sizeof(temp)) {
        return 0;
    }
    if (!fuzz_byte_buffer_append(buffer,
                                 (unsigned char const *)(uintptr_t)temp,
                                 (size_t)n)) {
        return 0;
    }

    for (i = 0u; i < (unsigned char)(extra_digits % 6u); ++i) {
        if (!fuzz_byte_buffer_append_u8(buffer,
                                        (unsigned char)('0' + (i % 10u)))) {
            return 0;
        }
    }

    return 1;
}

static int
fuzz_build_pnm_ascii_payload(uint8_t const *data,
                             size_t size,
                             fuzz_byte_buffer_t *payload)
{
    static char const *magic_types[] = {
        "P1", "P2", "P3", "P4", "P5", "P6", "P7"
    };

    fuzz_cursor_t cursor;
    uint32_t width;
    uint32_t height;
    uint32_t maxval;
    size_t comments;
    size_t i;
    unsigned char sample_limit;
    unsigned char format_index;
    char const *magic;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    format_index = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) %
                                   (sizeof(magic_types) / sizeof(magic_types[0])));
    magic = magic_types[format_index];

    width = (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u);
    height = (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u);
    maxval = (uint32_t)fuzz_cursor_take_u16be(&cursor, 255u);

    if (!fuzz_append_cstr(payload, magic) ||
        !fuzz_append_cstr(payload, "\n")) {
        return 0;
    }

    comments = (size_t)(fuzz_cursor_take_u8(&cursor, 0u) % 4u);
    for (i = 0u; i < comments; ++i) {
        if (!fuzz_append_cstr(payload, "# fuzz-comment ") ||
            !fuzz_append_decimal_token(payload,
                                       (uint32_t)fuzz_cursor_take_u32be(&cursor, 0u),
                                       fuzz_cursor_take_u8(&cursor, 0u)) ||
            !fuzz_append_cstr(payload, "\n")) {
            return 0;
        }
    }

    if (format_index == 6u) {
        if (!fuzz_append_cstr(payload, "WIDTH ") ||
            !fuzz_append_decimal_token(payload,
                                       width,
                                       fuzz_cursor_take_u8(&cursor, 0u)) ||
            !fuzz_append_cstr(payload, "\nHEIGHT ") ||
            !fuzz_append_decimal_token(payload,
                                       height,
                                       fuzz_cursor_take_u8(&cursor, 0u)) ||
            !fuzz_append_cstr(payload, "\nDEPTH ") ||
            !fuzz_append_decimal_token(payload,
                                       (uint32_t)(1u +
                                                  (fuzz_cursor_take_u8(&cursor, 0u) % 4u)),
                                       fuzz_cursor_take_u8(&cursor, 0u)) ||
            !fuzz_append_cstr(payload, "\nMAXVAL ") ||
            !fuzz_append_decimal_token(payload,
                                       maxval,
                                       fuzz_cursor_take_u8(&cursor, 0u)) ||
            !fuzz_append_cstr(payload, "\nTUPLTYPE RGB\nENDHDR\n")) {
            return 0;
        }

        if (fuzz_cursor_remaining(&cursor) > 0u) {
            return fuzz_byte_buffer_append(payload,
                                           cursor.data + cursor.pos,
                                           fuzz_cursor_remaining(&cursor));
        }
        return fuzz_append_cstr(payload, "0 0 0\n");
    }

    if (!fuzz_append_decimal_token(payload,
                                   width,
                                   fuzz_cursor_take_u8(&cursor, 0u)) ||
        !fuzz_append_cstr(payload, " ") ||
        !fuzz_append_decimal_token(payload,
                                   height,
                                   fuzz_cursor_take_u8(&cursor, 0u)) ||
        !fuzz_append_cstr(payload, "\n")) {
        return 0;
    }

    if (format_index == 1u || format_index == 2u ||
        format_index == 4u || format_index == 5u) {
        if (!fuzz_append_decimal_token(payload,
                                       maxval,
                                       fuzz_cursor_take_u8(&cursor, 0u)) ||
            !fuzz_append_cstr(payload, "\n")) {
            return 0;
        }
    }

    if (format_index <= 2u) {
        sample_limit = (unsigned char)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 64u));
        for (i = 0u; i < (size_t)sample_limit; ++i) {
            if (!fuzz_append_decimal_token(payload,
                                           (uint32_t)fuzz_cursor_take_u32be(&cursor, 0u),
                                           fuzz_cursor_take_u8(&cursor, 0u)) ||
                !fuzz_append_cstr(payload,
                                  (i % 9u == 8u) ? "\n" : " ")) {
                return 0;
            }
        }
        return 1;
    }

    if (fuzz_cursor_remaining(&cursor) > 0u) {
        return fuzz_byte_buffer_append(payload,
                                       cursor.data + cursor.pos,
                                       fuzz_cursor_remaining(&cursor));
    }

    return fuzz_byte_buffer_append_u8(payload, 0u);
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
    if (!fuzz_build_pnm_ascii_payload(data, size, &payload)) {
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
