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
#include <stdlib.h>
#include <string.h>

#include "loader-common.h"
#include "fuzz-loader-builtin-struct-common.h"

static int
fuzz_append_tiff_u16(fuzz_byte_buffer_t *payload,
                     int little_endian,
                     uint16_t value)
{
    if (payload == NULL) {
        return 0;
    }

    if (little_endian) {
        return fuzz_byte_buffer_append_u16le(payload, value);
    }
    return fuzz_byte_buffer_append_u16be(payload, value);
}

static int
fuzz_append_tiff_u32(fuzz_byte_buffer_t *payload,
                     int little_endian,
                     uint32_t value)
{
    if (payload == NULL) {
        return 0;
    }

    if (little_endian) {
        return fuzz_byte_buffer_append_u32le(payload, value);
    }
    return fuzz_byte_buffer_append_u32be(payload, value);
}

static uint32_t
fuzz_pack_orientation_value(int little_endian, uint16_t orientation)
{
    uint32_t packed;

    packed = (uint32_t)orientation;
    if (!little_endian) {
        packed <<= 16u;
    }

    return packed;
}

static int
fuzz_append_ifd_entry(fuzz_byte_buffer_t *payload,
                      int little_endian,
                      uint16_t tag,
                      uint16_t type,
                      uint32_t count,
                      uint32_t value)
{
    if (payload == NULL) {
        return 0;
    }

    if (!fuzz_append_tiff_u16(payload, little_endian, tag) ||
        !fuzz_append_tiff_u16(payload, little_endian, type) ||
        !fuzz_append_tiff_u32(payload, little_endian, count) ||
        !fuzz_append_tiff_u32(payload, little_endian, value)) {
        return 0;
    }

    return 1;
}

static int
fuzz_build_exif_orientation_payload(uint8_t const *data,
                                    size_t size,
                                    fuzz_byte_buffer_t *payload)
{
    enum {
        FUZZ_MAX_INPUT_BYTES = 256 * 1024,
        FUZZ_MAX_IFD_OFFSET = 512,
        FUZZ_MAX_TAIL_BYTES = 96
    };

    static unsigned char const exif_header[6] = {
        'E', 'x', 'i', 'f', 0x00u, 0x00u
    };

    fuzz_cursor_t cursor;
    int little_endian;
    int include_exif_header;
    unsigned char scenario;
    uint32_t ifd_offset;
    uint16_t entry_count_field;
    size_t entry_write_count;
    size_t i;
    uint16_t tag;
    uint16_t type;
    uint32_t count;
    uint32_t value_field;
    uint16_t orientation;
    int use_indirect_orientation;
    int force_invalid_indirect;
    uint32_t orientation_data_offset;
    uint32_t next_ifd_offset;
    unsigned char const *tail_data;
    size_t tail_size;

    if (data == NULL || payload == NULL ||
        size > (size_t)FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }

    little_endian = 0;
    include_exif_header = 0;
    scenario = 0u;
    ifd_offset = 8u;
    entry_count_field = 1u;
    entry_write_count = 1u;
    i = 0u;
    tag = 0u;
    type = 0u;
    count = 0u;
    value_field = 0u;
    orientation = 1u;
    use_indirect_orientation = 0;
    force_invalid_indirect = 0;
    orientation_data_offset = 0u;
    next_ifd_offset = 0u;
    tail_data = NULL;
    tail_size = 0u;

    fuzz_cursor_init(&cursor, data, size);

    include_exif_header = (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u;
    little_endian = (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u;
    scenario = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 8u);
    orientation = (uint16_t)(1u + (uint16_t)(fuzz_cursor_take_u8(&cursor, 0u)
                                             % 8u));

    switch (scenario) {
    case 0u:
        /* Valid orientation stored directly in the value field. */
        ifd_offset = 8u;
        entry_count_field = 1u;
        entry_write_count = 1u;
        break;
    case 1u:
        /* Reproduce the underflow precondition: IFD offset before TIFF body. */
        ifd_offset = 0u;
        entry_count_field = 1u;
        entry_write_count = 0u;
        break;
    case 2u:
        ifd_offset = 4u;
        entry_count_field = 1u;
        entry_write_count = 0u;
        break;
    case 3u:
        ifd_offset = 7u;
        entry_count_field = 1u;
        entry_write_count = 0u;
        break;
    case 4u:
        /* Oversized entry count to stress bounds checks. */
        ifd_offset = 8u;
        entry_count_field = (uint16_t)(0x0100u +
                          (uint16_t)fuzz_cursor_take_u8(&cursor, 0u));
        entry_write_count = 0u;
        break;
    case 5u:
        /* Orientation points to an external value (count > 1). */
        ifd_offset = 8u;
        entry_count_field = 1u;
        entry_write_count = 1u;
        use_indirect_orientation = 1;
        force_invalid_indirect = (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) !=
            0u;
        break;
    case 6u:
        /* No orientation tag, but structured entries are still present. */
        ifd_offset = 8u;
        entry_count_field = 2u;
        entry_write_count = 2u;
        break;
    case 7u:
    default:
        ifd_offset = (uint32_t)(fuzz_cursor_take_u8(&cursor, 0u) %
                                FUZZ_MAX_IFD_OFFSET);
        entry_count_field = (uint16_t)(fuzz_cursor_take_u8(&cursor, 0u) % 6u);
        entry_write_count = (size_t)entry_count_field;
        if (entry_write_count > 4u) {
            entry_write_count = 4u;
        }
        break;
    }

    if (include_exif_header) {
        if (!fuzz_byte_buffer_append(payload,
                                     exif_header,
                                     sizeof(exif_header))) {
            return 0;
        }
    }

    if (little_endian) {
        if (!fuzz_byte_buffer_append_u8(payload, (unsigned char)'I') ||
            !fuzz_byte_buffer_append_u8(payload, (unsigned char)'I')) {
            return 0;
        }
    } else {
        if (!fuzz_byte_buffer_append_u8(payload, (unsigned char)'M') ||
            !fuzz_byte_buffer_append_u8(payload, (unsigned char)'M')) {
            return 0;
        }
    }

    if (!fuzz_append_tiff_u16(payload, little_endian, 42u) ||
        !fuzz_append_tiff_u32(payload, little_endian, ifd_offset)) {
        return 0;
    }

    if (ifd_offset > 8u) {
        size_t pad;

        pad = (size_t)(ifd_offset - 8u);
        if (!fuzz_byte_buffer_append_zeros(payload, pad)) {
            return 0;
        }
    }

    if (ifd_offset >= 8u) {
        if (!fuzz_append_tiff_u16(payload,
                                  little_endian,
                                  entry_count_field)) {
            return 0;
        }

        for (i = 0u; i < entry_write_count; ++i) {
            tag = fuzz_cursor_take_u16be(&cursor, 0u);
            type = (uint16_t)(1u + (uint16_t)(fuzz_cursor_take_u8(&cursor, 0u)
                                              % 12u));
            count = 1u + (uint32_t)(fuzz_cursor_take_u8(&cursor, 0u) % 4u);
            value_field = fuzz_cursor_take_u32be(&cursor, 0u);

            if ((scenario == 0u || scenario == 5u) && i == 0u) {
                tag = 0x0112u;
                type = 3u;
                if (use_indirect_orientation) {
                    count = 2u;
                    orientation_data_offset = ifd_offset + 2u +
                        (uint32_t)(entry_write_count * 12u) + 4u;
                    if (force_invalid_indirect) {
                        value_field = (uint32_t)(fuzz_cursor_take_u8(&cursor,
                                                                     0u) % 8u);
                    } else {
                        value_field = orientation_data_offset;
                    }
                } else {
                    count = 1u;
                    value_field = fuzz_pack_orientation_value(little_endian,
                                                              orientation);
                }
            } else if (scenario == 6u) {
                tag = (uint16_t)(0x010eu + (uint16_t)i);
                type = 2u;
                count = 4u;
            }

            if (!fuzz_append_ifd_entry(payload,
                                       little_endian,
                                       tag,
                                       type,
                                       count,
                                       value_field)) {
                return 0;
            }
        }

        next_ifd_offset = fuzz_cursor_take_u32be(&cursor, 0u);
        if (!fuzz_append_tiff_u32(payload,
                                  little_endian,
                                  next_ifd_offset)) {
            return 0;
        }

        if (use_indirect_orientation && !force_invalid_indirect) {
            if (orientation_data_offset > payload->size) {
                if (!fuzz_byte_buffer_append_zeros(payload,
                                                   orientation_data_offset -
                                                   payload->size)) {
                    return 0;
                }
            }
            if (!fuzz_append_tiff_u16(payload,
                                      little_endian,
                                      orientation)) {
                return 0;
            }
        }
    }

    if (fuzz_cursor_remaining(&cursor) > 0u) {
        tail_data = cursor.data + cursor.pos;
        tail_size = fuzz_cursor_remaining(&cursor);
        if (tail_size > (size_t)FUZZ_MAX_TAIL_BYTES) {
            tail_size = (size_t)FUZZ_MAX_TAIL_BYTES;
        }
        if (!fuzz_byte_buffer_append(payload, tail_data, tail_size)) {
            return 0;
        }
    }

    return 1;
}

static void
fuzz_exercise_orientation_parser(unsigned char const *data, size_t size)
{
    int found;
    int orientation;

    found = 0;
    orientation = 0;

    if (data == NULL) {
        return;
    }

    found = loader_exif_parse_orientation(data, size, &orientation);
    if (found != 0 && (orientation < 1 || orientation > 8)) {
        abort();
    }
}

int
LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
    enum { FUZZ_MAX_INPUT_BYTES = 1 * 1024 * 1024 };

    fuzz_byte_buffer_t payload;

    if (data == NULL || size > (size_t)FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }

    fuzz_exercise_orientation_parser(data, size);

    fuzz_byte_buffer_init(&payload);
    if (!fuzz_build_exif_orientation_payload(data, size, &payload)) {
        fuzz_byte_buffer_reset(&payload);
        return 0;
    }

    fuzz_exercise_orientation_parser(payload.data, payload.size);
    if (payload.size > 6u &&
        memcmp(payload.data, "Exif\0\0", 6u) == 0) {
        fuzz_exercise_orientation_parser(payload.data + 6u,
                                        payload.size - 6u);
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
