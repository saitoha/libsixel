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
fuzz_build_png_apng_payload(uint8_t const *data,
                            size_t size,
                            fuzz_byte_buffer_t *payload)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    static unsigned char const fallback_idat[8] = {
        0x78u, 0x9cu, 0x63u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u
    };

    fuzz_cursor_t cursor;
    unsigned char ihdr[13];
    unsigned char actl[8];
    uint32_t canvas_width;
    uint32_t canvas_height;
    uint32_t seq;
    uint32_t frame_count;
    uint32_t i;
    int wrote_frame_payload;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    if (!fuzz_byte_buffer_append(payload, png_signature, sizeof(png_signature))) {
        return 0;
    }

    canvas_width = 1u + (uint32_t)(fuzz_cursor_take_u8(&cursor, 0u) % 64u);
    canvas_height = 1u + (uint32_t)(fuzz_cursor_take_u8(&cursor, 0u) % 64u);

    ihdr[0] = (unsigned char)((canvas_width >> 24) & 0xffu);
    ihdr[1] = (unsigned char)((canvas_width >> 16) & 0xffu);
    ihdr[2] = (unsigned char)((canvas_width >> 8) & 0xffu);
    ihdr[3] = (unsigned char)(canvas_width & 0xffu);
    ihdr[4] = (unsigned char)((canvas_height >> 24) & 0xffu);
    ihdr[5] = (unsigned char)((canvas_height >> 16) & 0xffu);
    ihdr[6] = (unsigned char)((canvas_height >> 8) & 0xffu);
    ihdr[7] = (unsigned char)(canvas_height & 0xffu);
    ihdr[8] = 8u;
    ihdr[9] = 6u;
    ihdr[10] = 0u;
    ihdr[11] = 0u;
    ihdr[12] = 0u;

    if (!fuzz_append_png_chunk(payload,
                               "IHDR",
                               ihdr,
                               sizeof(ihdr),
                               (int)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u),
                               (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u))) {
        return 0;
    }

    frame_count = 1u + (uint32_t)(fuzz_cursor_take_u8(&cursor, 0u) % 3u);
    actl[0] = (unsigned char)((frame_count >> 24) & 0xffu);
    actl[1] = (unsigned char)((frame_count >> 16) & 0xffu);
    actl[2] = (unsigned char)((frame_count >> 8) & 0xffu);
    actl[3] = (unsigned char)(frame_count & 0xffu);
    actl[4] = 0u;
    actl[5] = 0u;
    actl[6] = 0u;
    actl[7] = fuzz_cursor_take_u8(&cursor, 0u);

    if ((fuzz_cursor_take_u8(&cursor, 1u) & 0x01u) != 0u) {
        if (!fuzz_append_png_chunk(payload,
                                   "acTL",
                                   actl,
                                   sizeof(actl),
                                   (int)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u),
                                   (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u))) {
            return 0;
        }
    }

    seq = fuzz_cursor_take_u32be(&cursor, 0u);
    wrote_frame_payload = 0;
    for (i = 0u; i < frame_count; ++i) {
        unsigned char fctl[26];
        unsigned char const *frame_bytes;
        size_t frame_size;
        uint32_t frame_width;
        uint32_t frame_height;
        uint32_t frame_seq;

        frame_width = 1u + (uint32_t)(fuzz_cursor_take_u8(&cursor, 0u) % 64u);
        frame_height = 1u + (uint32_t)(fuzz_cursor_take_u8(&cursor, 0u) % 64u);
        frame_seq = seq;
        seq += 1u;

        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
            frame_seq ^= (uint32_t)fuzz_cursor_take_u8(&cursor, 0u);
        }

        fctl[0] = (unsigned char)((frame_seq >> 24) & 0xffu);
        fctl[1] = (unsigned char)((frame_seq >> 16) & 0xffu);
        fctl[2] = (unsigned char)((frame_seq >> 8) & 0xffu);
        fctl[3] = (unsigned char)(frame_seq & 0xffu);
        fctl[4] = (unsigned char)((frame_width >> 24) & 0xffu);
        fctl[5] = (unsigned char)((frame_width >> 16) & 0xffu);
        fctl[6] = (unsigned char)((frame_width >> 8) & 0xffu);
        fctl[7] = (unsigned char)(frame_width & 0xffu);
        fctl[8] = (unsigned char)((frame_height >> 24) & 0xffu);
        fctl[9] = (unsigned char)((frame_height >> 16) & 0xffu);
        fctl[10] = (unsigned char)((frame_height >> 8) & 0xffu);
        fctl[11] = (unsigned char)(frame_height & 0xffu);
        fctl[12] = 0u;
        fctl[13] = 0u;
        fctl[14] = 0u;
        fctl[15] = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 4u);
        fctl[16] = 0u;
        fctl[17] = 0u;
        fctl[18] = 0u;
        fctl[19] = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 4u);
        fctl[20] = 0u;
        fctl[21] = fuzz_cursor_take_u8(&cursor, 1u);
        fctl[22] = 0u;
        fctl[23] = fuzz_cursor_take_u8(&cursor, 0u);
        fctl[24] = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 3u);
        fctl[25] = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 2u);

        if ((fuzz_cursor_take_u8(&cursor, 1u) & 0x01u) != 0u) {
            if (!fuzz_append_png_chunk(payload,
                                       "fcTL",
                                       fctl,
                                       sizeof(fctl),
                                       (int)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u),
                                       (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u))) {
                return 0;
            }
        }

        frame_bytes = NULL;
        frame_size = 0u;
        if (fuzz_cursor_remaining(&cursor) > 0u) {
            frame_size = (size_t)(fuzz_cursor_take_u16be(&cursor, 0u) % 1025u);
            if (frame_size > fuzz_cursor_remaining(&cursor)) {
                frame_size = fuzz_cursor_remaining(&cursor);
            }
            frame_bytes = cursor.data + cursor.pos;
            cursor.pos += frame_size;
        }

        if (i == 0u && (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) == 0u) {
            if (frame_size == 0u) {
                frame_bytes = fallback_idat;
                frame_size = sizeof(fallback_idat);
            }
            if (!fuzz_append_png_chunk(payload,
                                       "IDAT",
                                       frame_bytes,
                                       frame_size,
                                       (int)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u),
                                       (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u))) {
                return 0;
            }
            wrote_frame_payload = 1;
        } else {
            fuzz_byte_buffer_t fdat;
            fuzz_byte_buffer_init(&fdat);

            if (!fuzz_byte_buffer_append_u32be(&fdat, seq++) ||
                !fuzz_byte_buffer_append(&fdat,
                                         frame_bytes,
                                         frame_size)) {
                fuzz_byte_buffer_reset(&fdat);
                return 0;
            }

            if (!fuzz_append_png_chunk(payload,
                                       "fdAT",
                                       fdat.data,
                                       fdat.size,
                                       (int)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u),
                                       (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u))) {
                fuzz_byte_buffer_reset(&fdat);
                return 0;
            }
            fuzz_byte_buffer_reset(&fdat);
            wrote_frame_payload = 1;
        }
    }

    if (!wrote_frame_payload) {
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
    if (!fuzz_build_png_apng_payload(data, size, &payload)) {
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
