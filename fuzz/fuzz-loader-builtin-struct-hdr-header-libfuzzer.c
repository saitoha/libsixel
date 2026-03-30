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
fuzz_hdr_append_line(fuzz_byte_buffer_t *buffer, char const *line)
{
    size_t len;

    if (buffer == NULL || line == NULL) {
        return 0;
    }

    len = 0u;
    while (line[len] != '\0') {
        ++len;
    }

    return fuzz_byte_buffer_append(buffer,
                                   (unsigned char const *)(uintptr_t)line,
                                   len) &&
           fuzz_byte_buffer_append_u8(buffer, '\n');
}

static int
fuzz_build_hdr_header_payload(uint8_t const *data,
                              size_t size,
                              fuzz_byte_buffer_t *payload)
{
    fuzz_cursor_t cursor;
    char line[160];
    int width;
    int height;
    size_t tail_size;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    width = 1 + (int)(fuzz_cursor_take_u8(&cursor, 0u) % 64u);
    height = 1 + (int)(fuzz_cursor_take_u8(&cursor, 0u) % 64u);

    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
        width = (int)(fuzz_cursor_take_u16be(&cursor, (uint16_t)width));
        height = (int)(fuzz_cursor_take_u16be(&cursor, (uint16_t)height));
    }

    if (width <= 0) {
        width = 1;
    }
    if (height <= 0) {
        height = 1;
    }

    if (!fuzz_hdr_append_line(payload,
                              (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u
                                  ? "#?RADIANCE"
                                  : "#?RGBE")) {
        return 0;
    }

    if (snprintf(line,
                 sizeof(line),
                 "FORMAT=%s",
                 (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u
                     ? "32-bit_rle_rgbe"
                     : "32-bit_rle_xyze") < 0 ||
        !fuzz_hdr_append_line(payload, line)) {
        return 0;
    }

    if (snprintf(line,
                 sizeof(line),
                 "GAMMA=0.%u",
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 100u)) < 0 ||
        !fuzz_hdr_append_line(payload, line)) {
        return 0;
    }

    if (snprintf(line,
                 sizeof(line),
                 "EXPOSURE=%u.%02u",
                 (unsigned int)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 8u)),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 100u)) < 0 ||
        !fuzz_hdr_append_line(payload, line)) {
        return 0;
    }

    if (snprintf(line,
                 sizeof(line),
                 "COLORCORR=%u.%u %u.%u %u.%u",
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 4u),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 100u),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 4u),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 100u),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 4u),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 100u)) < 0 ||
        !fuzz_hdr_append_line(payload, line)) {
        return 0;
    }

    if (snprintf(line,
                 sizeof(line),
                 "PRIMARIES=%u.%u %u.%u %u.%u %u.%u",
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 2u),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 100u),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 2u),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 100u),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 2u),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 100u),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 2u),
                 (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 100u)) < 0 ||
        !fuzz_hdr_append_line(payload, line)) {
        return 0;
    }

    if (!fuzz_byte_buffer_append_u8(payload, '\n')) {
        return 0;
    }

    if (snprintf(line,
                 sizeof(line),
                 "%c%c %d %c%c %d",
                 (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u ? '-' : '+',
                 (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u ? 'Y' : 'X',
                 height,
                 (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u ? '+' : '-',
                 (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u ? 'X' : 'Y',
                 width) < 0 ||
        !fuzz_hdr_append_line(payload, line)) {
        return 0;
    }

    tail_size = fuzz_cursor_remaining(&cursor);
    if (tail_size > 0u) {
        if (!fuzz_byte_buffer_append(payload,
                                     cursor.data + cursor.pos,
                                     tail_size)) {
            return 0;
        }
    } else {
        size_t fallback_pixels;

        fallback_pixels = (size_t)width * (size_t)height;
        if (fallback_pixels > 64u) {
            fallback_pixels = 64u;
        }
        if (!fuzz_byte_buffer_append_zeros(payload,
                                           fallback_pixels * 4u)) {
            return 0;
        }
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
    if (!fuzz_build_hdr_header_payload(data, size, &payload)) {
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
