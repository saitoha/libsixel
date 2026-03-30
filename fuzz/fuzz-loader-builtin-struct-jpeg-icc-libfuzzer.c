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

static unsigned char const g_base_jpeg[] = {
  0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
  0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
  0x00, 0x03, 0x02, 0x02, 0x03, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x04,
  0x03, 0x03, 0x04, 0x05, 0x08, 0x05, 0x05, 0x04, 0x04, 0x05, 0x0a, 0x07,
  0x07, 0x06, 0x08, 0x0c, 0x0a, 0x0c, 0x0c, 0x0b, 0x0a, 0x0b, 0x0b, 0x0d,
  0x0e, 0x12, 0x10, 0x0d, 0x0e, 0x11, 0x0e, 0x0b, 0x0b, 0x10, 0x16, 0x10,
  0x11, 0x13, 0x14, 0x15, 0x15, 0x15, 0x0c, 0x0f, 0x17, 0x18, 0x16, 0x14,
  0x18, 0x12, 0x14, 0x15, 0x14, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x03, 0x04,
  0x04, 0x05, 0x04, 0x05, 0x09, 0x05, 0x05, 0x09, 0x14, 0x0d, 0x0b, 0x0d,
  0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
  0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
  0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
  0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
  0x14, 0x14, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x08, 0x00, 0x0c, 0x03,
  0x01, 0x11, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
  0x15, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x07, 0xff, 0xc4, 0x00, 0x24,
  0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x03, 0x05, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x05, 0x03, 0x04, 0x11, 0x00,
  0x06, 0x12, 0x14, 0x21, 0x31, 0x07, 0x41, 0x51, 0x71, 0x91, 0xff, 0xc4,
  0x00, 0x17, 0x01, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x05, 0x07, 0xff,
  0xc4, 0x00, 0x21, 0x11, 0x00, 0x01, 0x03, 0x04, 0x01, 0x05, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x11,
  0x00, 0x21, 0x71, 0x91, 0x05, 0x12, 0x22, 0x23, 0x61, 0x81, 0xff, 0xda,
  0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00,
  0x98, 0xdb, 0x6e, 0xd8, 0xe8, 0x68, 0x6b, 0xbb, 0xa8, 0xf8, 0xbb, 0x97,
  0x94, 0x5a, 0x44, 0xd2, 0xb4, 0xac, 0xae, 0xca, 0xcd, 0x9f, 0x01, 0x90,
  0x11, 0xf3, 0x8c, 0x91, 0xdf, 0xf3, 0x59, 0x7a, 0x1e, 0xe4, 0x12, 0xe8,
  0x4b, 0xad, 0x8c, 0x8b, 0x8c, 0x90, 0x6f, 0xf2, 0xae, 0x38, 0x92, 0xb1,
  0xda, 0xa8, 0x18, 0xbe, 0xe9, 0x0c, 0x77, 0xaa, 0xf2, 0x7d, 0x25, 0x32,
  0x9b, 0x6a, 0xa8, 0xe4, 0x03, 0x37, 0x5b, 0x77, 0x4e, 0x83, 0xf2, 0x23,
  0x27, 0x0b, 0xec, 0x33, 0xd8, 0x78, 0xfa, 0xd3, 0xa8, 0x1c, 0xa9, 0x1e,
  0x25, 0x20, 0x0f, 0x60, 0x93, 0x99, 0x91, 0xa8, 0xb5, 0x0e, 0x96, 0xc0,
  0x83, 0x3b, 0xaf, 0xff, 0xd9
};

static int
fuzz_append_jpeg_app_segment(fuzz_byte_buffer_t *buffer,
                             unsigned char marker,
                             unsigned char const *segment_data,
                             size_t segment_size)
{
    uint16_t segment_length;

    if (buffer == NULL || segment_data == NULL) {
        return 0;
    }
    if (segment_size > 65533u) {
        return 0;
    }

    segment_length = (uint16_t)(segment_size + 2u);
    return fuzz_byte_buffer_append_u8(buffer, 0xffu) &&
           fuzz_byte_buffer_append_u8(buffer, marker) &&
           fuzz_byte_buffer_append_u16be(buffer, segment_length) &&
           fuzz_byte_buffer_append(buffer, segment_data, segment_size);
}

static int
fuzz_emit_icc_app2_segments(fuzz_byte_buffer_t *out,
                            fuzz_cursor_t *cursor)
{
    static unsigned char const fallback_profile[32] = {
        0x00u, 0x00u, 0x02u, 0x10u, 0x6cu, 0x63u, 0x6du, 0x73u,
        0x04u, 0x20u, 0x00u, 0x00u, 0x6du, 0x6e, 0x74u, 0x72u,
        0x52u, 0x47u, 0x42u, 0x20u, 0x58u, 0x59u, 0x5au, 0x20u,
        0x07u, 0xe8u, 0x00u, 0x02u, 0x00u, 0x01u, 0x00u, 0x00u
    };

    unsigned char const *profile;
    size_t profile_size;
    size_t max_profile_size;
    uint8_t declared_total;
    uint8_t real_total;
    size_t chunk_size[4];
    size_t chunk_offset[4];
    size_t consumed;
    size_t emit_count;
    size_t i;

    if (out == NULL || cursor == NULL) {
        return 0;
    }

    max_profile_size = 384u;
    if (fuzz_cursor_remaining(cursor) > 0u) {
        size_t requested;

        requested = (size_t)fuzz_cursor_take_u16be(cursor,
                                                   (uint16_t)max_profile_size);
        if (requested == 0u || requested > max_profile_size) {
            requested = max_profile_size;
        }
        profile_size = requested;
        if (profile_size > fuzz_cursor_remaining(cursor)) {
            profile_size = fuzz_cursor_remaining(cursor);
        }
        profile = cursor->data + cursor->pos;
        cursor->pos += profile_size;
    } else {
        profile = fallback_profile;
        profile_size = sizeof(fallback_profile);
    }

    real_total = (uint8_t)(1u + (fuzz_cursor_take_u8(cursor, 0u) % 4u));
    declared_total = (uint8_t)(1u + (fuzz_cursor_take_u8(cursor, 0u) % 4u));
    if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) == 0u) {
        declared_total = real_total;
    }

    for (i = 0u; i < 4u; ++i) {
        chunk_size[i] = 0u;
        chunk_offset[i] = 0u;
    }

    consumed = 0u;
    for (i = 0u; i < (size_t)real_total; ++i) {
        size_t remaining_chunks;
        size_t remaining_profile;
        size_t next_size;

        chunk_offset[i] = consumed;
        remaining_chunks = (size_t)real_total - i;
        remaining_profile = profile_size - consumed;
        if (remaining_chunks <= 1u) {
            next_size = remaining_profile;
        } else {
            next_size = (size_t)(fuzz_cursor_take_u8(cursor, 0u) %
                                 (remaining_profile + 1u));
            if (next_size == 0u) {
                next_size = remaining_profile / remaining_chunks;
            }
            if (next_size > remaining_profile) {
                next_size = remaining_profile;
            }
        }
        chunk_size[i] = next_size;
        consumed += next_size;
    }

    emit_count = (size_t)real_total;
    if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u) {
        emit_count += 1u;
    }

    for (i = 0u; i < emit_count; ++i) {
        fuzz_byte_buffer_t segment;
        uint8_t logical_index;
        uint8_t seq_no;
        size_t src_index;

        logical_index = (uint8_t)(i % (size_t)real_total);
        if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u) {
            logical_index = (uint8_t)(fuzz_cursor_take_u8(cursor, logical_index) %
                                      real_total);
        }
        seq_no = (uint8_t)(logical_index + 1u);
        if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u) {
            seq_no ^= (uint8_t)(fuzz_cursor_take_u8(cursor, 0u) & 0x0fu);
        }

        src_index = (size_t)logical_index;
        fuzz_byte_buffer_init(&segment);

        if (!fuzz_byte_buffer_append(&segment,
                                     (unsigned char const *)"ICC_PROFILE\0",
                                     12u) ||
            !fuzz_byte_buffer_append_u8(&segment, seq_no) ||
            !fuzz_byte_buffer_append_u8(&segment, declared_total) ||
            !fuzz_byte_buffer_append(&segment,
                                     profile + chunk_offset[src_index],
                                     chunk_size[src_index]) ||
            !fuzz_append_jpeg_app_segment(out, 0xe2u, segment.data, segment.size)) {
            fuzz_byte_buffer_reset(&segment);
            return 0;
        }

        fuzz_byte_buffer_reset(&segment);
    }

    return 1;
}

static int
fuzz_build_jpeg_icc_payload(uint8_t const *data,
                            size_t size,
                            fuzz_byte_buffer_t *payload)
{
    fuzz_cursor_t cursor;

    if (payload == NULL || sizeof(g_base_jpeg) < 4u) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    if (!fuzz_byte_buffer_append(payload, g_base_jpeg, 2u)) {
        return 0;
    }

    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
        fuzz_byte_buffer_t malformed;
        fuzz_byte_buffer_init(&malformed);
        if (!fuzz_byte_buffer_append(&malformed,
                                     (unsigned char const *)"ICC_PROFILX\0",
                                     12u) ||
            !fuzz_byte_buffer_append_u8(&malformed,
                                        fuzz_cursor_take_u8(&cursor, 1u)) ||
            !fuzz_byte_buffer_append_u8(&malformed,
                                        fuzz_cursor_take_u8(&cursor, 1u)) ||
            !fuzz_append_jpeg_app_segment(payload,
                                          0xe2u,
                                          malformed.data,
                                          malformed.size)) {
            fuzz_byte_buffer_reset(&malformed);
            return 0;
        }
        fuzz_byte_buffer_reset(&malformed);
    }

    if (!fuzz_emit_icc_app2_segments(payload, &cursor)) {
        return 0;
    }

    return fuzz_byte_buffer_append(payload,
                                   g_base_jpeg + 2u,
                                   sizeof(g_base_jpeg) - 2u);
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
    if (!fuzz_build_jpeg_icc_payload(data, size, &payload)) {
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
