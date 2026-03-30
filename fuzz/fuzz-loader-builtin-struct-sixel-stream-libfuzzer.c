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
#include <stdarg.h>
#include <stdio.h>

#include "fuzz-loader-builtin-struct-common.h"

static int
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
fuzz_append_printf(fuzz_byte_buffer_t *buffer,
                   char const *fmt, ...)
{
    char temp[128];
    int n;
    va_list ap;

    if (buffer == NULL || fmt == NULL) {
        return 0;
    }

    va_start(ap, fmt);
    n = vsnprintf(temp, sizeof(temp), fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= sizeof(temp)) {
        return 0;
    }

    return fuzz_byte_buffer_append(buffer,
                                   (unsigned char const *)temp,
                                   (size_t)n);
}

static int
fuzz_build_sixel_stream_payload(uint8_t const *data,
                                size_t size,
                                fuzz_byte_buffer_t *payload)
{
    fuzz_cursor_t cursor;
    size_t token_count;
    size_t i;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    token_count = 8u + (size_t)(fuzz_cursor_take_u8(&cursor, 0u) % 128u);

    if (!fuzz_byte_buffer_append(payload,
                                 (unsigned char const *)"\x1bPq",
                                 3u)) {
        return 0;
    }

    for (i = 0u; i < token_count; ++i) {
        unsigned char kind;

        kind = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 6u);
        switch (kind) {
        case 0u: {
            unsigned char ch;
            ch = (unsigned char)(0x3fu + (fuzz_cursor_take_u8(&cursor, 0u) % 64u));
            if (!fuzz_byte_buffer_append_u8(payload, ch)) {
                return 0;
            }
            break;
        }
        case 1u:
            if (!fuzz_append_printf(payload,
                                    "#%u;2;%u;%u;%u",
                                    (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 32u),
                                    (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 101u),
                                    (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 101u),
                                    (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 101u))) {
                return 0;
            }
            break;
        case 2u:
            if (!fuzz_append_printf(payload,
                                    "!%u%c",
                                    (unsigned int)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 128u)),
                                    (unsigned int)(0x3fu + (fuzz_cursor_take_u8(&cursor, 0u) % 64u)))) {
                return 0;
            }
            break;
        case 3u:
            if (!fuzz_append_printf(payload,
                                    "\"%u;%u;%u;%u",
                                    (unsigned int)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 4u)),
                                    (unsigned int)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 4u)),
                                    (unsigned int)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 160u)),
                                    (unsigned int)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 160u)))) {
                return 0;
            }
            break;
        case 4u:
            if (!fuzz_byte_buffer_append_u8(payload,
                                            (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u
                                                ? (unsigned char)'$'
                                                : (unsigned char)'-')) {
                return 0;
            }
            break;
        default:
            if (!fuzz_byte_buffer_append_u8(payload,
                                            (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 2u
                                                                ? ';'
                                                                : '?'))) {
                return 0;
            }
            break;
        }
    }

    if (fuzz_cursor_remaining(&cursor) > 0u) {
        size_t tail_size;

        tail_size = fuzz_cursor_remaining(&cursor);
        if (tail_size > 128u) {
            tail_size = 128u;
        }
        if (!fuzz_byte_buffer_append(payload,
                                     cursor.data + cursor.pos,
                                     tail_size)) {
            return 0;
        }
    }

    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) == 0u) {
        return fuzz_byte_buffer_append(payload,
                                       (unsigned char const *)"\x1b\\",
                                       2u);
    }

    return fuzz_byte_buffer_append_u8(payload, 0x9cu);
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
    if (!fuzz_build_sixel_stream_payload(data, size, &payload)) {
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
