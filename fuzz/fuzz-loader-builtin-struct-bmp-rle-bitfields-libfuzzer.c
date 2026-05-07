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
#include <stdlib.h>

#include "fuzz-loader-builtin-struct-common.h"

extern size_t LLVMFuzzerMutate(uint8_t *data, size_t size, size_t max_size);

enum fuzz_bmp_rle_feature {
    FUZZ_BMP_RLE_FEATURE_HEADER = 0,
    FUZZ_BMP_RLE_FEATURE_RLE8,
    FUZZ_BMP_RLE_FEATURE_RLE4,
    FUZZ_BMP_RLE_FEATURE_BITFIELDS,
    FUZZ_BMP_RLE_FEATURE_ALPHA_BITFIELDS,
    FUZZ_BMP_RLE_FEATURE_OS2_HUFFMAN1D,
    FUZZ_BMP_RLE_FEATURE_OS2_RLE24,
    FUZZ_BMP_RLE_FEATURE_CMYK_RLE,
    FUZZ_BMP_RLE_FEATURE_CMYK_RAW,
    FUZZ_BMP_RLE_FEATURE_ENCODED_RUN,
    FUZZ_BMP_RLE_FEATURE_ABSOLUTE_RUN,
    FUZZ_BMP_RLE_FEATURE_DELTA,
    FUZZ_BMP_RLE_FEATURE_EOL,
    FUZZ_BMP_RLE_FEATURE_EOB,
    FUZZ_BMP_RLE_FEATURE_ESCAPE,
    FUZZ_BMP_RLE_FEATURE_PALETTE,
    FUZZ_BMP_RLE_FEATURE_MASKS,
    FUZZ_BMP_RLE_FEATURE_RAW_DATA,
    FUZZ_BMP_RLE_FEATURE_COUNT
};

static unsigned int g_bmp_rle_feature_mask = 0u;
static unsigned int g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_COUNT];
static int g_bmp_rle_stats_registered = 0;

#define FUZZ_BMP_RLE_RECORD_FEATURE(_feature) \
    do { \
        g_bmp_rle_feature_mask |= 1u << (_feature); \
        ++g_bmp_rle_feature_hits[(_feature)]; \
    } while (0)

static unsigned int
fuzz_bmp_rle_count_bits(unsigned int value)
{
    unsigned int count;

    count = 0u;
    while (value != 0u) {
        count += value & 1u;
        value >>= 1;
    }

    return count;
}

static void
fuzz_bmp_rle_note_feature(unsigned int feature)
{
    if (feature >= FUZZ_BMP_RLE_FEATURE_COUNT) {
        return;
    }

    /*
     * BMP's RLE and bitfields variants share shallow parser edges. These
     * milestones tell libFuzzer whether it reached each compression family
     * and each RLE escape command, not just the BMP header path.
     */
    switch (feature) {
    case FUZZ_BMP_RLE_FEATURE_HEADER:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_HEADER);
        break;
    case FUZZ_BMP_RLE_FEATURE_RLE8:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_RLE8);
        break;
    case FUZZ_BMP_RLE_FEATURE_RLE4:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_RLE4);
        break;
    case FUZZ_BMP_RLE_FEATURE_BITFIELDS:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_BITFIELDS);
        break;
    case FUZZ_BMP_RLE_FEATURE_ALPHA_BITFIELDS:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_ALPHA_BITFIELDS);
        break;
    case FUZZ_BMP_RLE_FEATURE_OS2_HUFFMAN1D:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_OS2_HUFFMAN1D);
        break;
    case FUZZ_BMP_RLE_FEATURE_OS2_RLE24:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_OS2_RLE24);
        break;
    case FUZZ_BMP_RLE_FEATURE_CMYK_RLE:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_CMYK_RLE);
        break;
    case FUZZ_BMP_RLE_FEATURE_CMYK_RAW:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_CMYK_RAW);
        break;
    case FUZZ_BMP_RLE_FEATURE_ENCODED_RUN:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_ENCODED_RUN);
        break;
    case FUZZ_BMP_RLE_FEATURE_ABSOLUTE_RUN:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_ABSOLUTE_RUN);
        break;
    case FUZZ_BMP_RLE_FEATURE_DELTA:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_DELTA);
        break;
    case FUZZ_BMP_RLE_FEATURE_EOL:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_EOL);
        break;
    case FUZZ_BMP_RLE_FEATURE_EOB:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_EOB);
        break;
    case FUZZ_BMP_RLE_FEATURE_ESCAPE:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_ESCAPE);
        break;
    case FUZZ_BMP_RLE_FEATURE_PALETTE:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_PALETTE);
        break;
    case FUZZ_BMP_RLE_FEATURE_MASKS:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_MASKS);
        break;
    case FUZZ_BMP_RLE_FEATURE_RAW_DATA:
        FUZZ_BMP_RLE_RECORD_FEATURE(FUZZ_BMP_RLE_FEATURE_RAW_DATA);
        break;
    default:
        break;
    }
}

static void
fuzz_bmp_rle_print_semantic_stats(void)
{
    fprintf(stderr,
            "bmp-rle-semantic-features: mask=0x%08x count=%u "
            "header=%u rle8=%u rle4=%u bitfields=%u alpha-bitfields=%u "
            "os2-huffman1d=%u os2-rle24=%u cmyk-rle=%u cmyk-raw=%u "
            "encoded-run=%u absolute-run=%u delta=%u eol=%u eob=%u "
            "escape=%u palette=%u masks=%u raw-data=%u\n",
            g_bmp_rle_feature_mask,
            fuzz_bmp_rle_count_bits(g_bmp_rle_feature_mask),
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_HEADER],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_RLE8],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_RLE4],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_BITFIELDS],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_ALPHA_BITFIELDS],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_OS2_HUFFMAN1D],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_OS2_RLE24],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_CMYK_RLE],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_CMYK_RAW],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_ENCODED_RUN],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_ABSOLUTE_RUN],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_DELTA],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_EOL],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_EOB],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_ESCAPE],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_PALETTE],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_MASKS],
            g_bmp_rle_feature_hits[FUZZ_BMP_RLE_FEATURE_RAW_DATA]);
}

static void
fuzz_bmp_rle_register_semantic_stats(void)
{
    if (g_bmp_rle_stats_registered) {
        return;
    }

    (void)atexit(fuzz_bmp_rle_print_semantic_stats);
    g_bmp_rle_stats_registered = 1;
}

static uint32_t
fuzz_bmp_rle_mutator_next(uint32_t *state)
{
    uint32_t value;

    value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    if (value == 0u) {
        value = UINT32_C(0xc2b2ae35);
    }
    *state = value;

    return value;
}

static int
fuzz_bmp_rle_mutator_append_u8(uint8_t *data,
                               size_t *pos,
                               size_t max_size,
                               unsigned char value)
{
    if (data == NULL || pos == NULL || *pos >= max_size) {
        return 0;
    }

    data[*pos] = value;
    ++*pos;
    return 1;
}

static int
fuzz_bmp_rle_mutator_append_noise(uint8_t *data,
                                  size_t *pos,
                                  size_t max_size,
                                  uint32_t *state,
                                  size_t count)
{
    size_t i;

    for (i = 0u; i < count; ++i) {
        if (!fuzz_bmp_rle_mutator_append_u8(
                data,
                pos,
                max_size,
                (unsigned char)fuzz_bmp_rle_mutator_next(state))) {
            return 0;
        }
    }

    return 1;
}

static int
fuzz_bmp_rle_mutator_append_rle_commands(uint8_t *data,
                                         size_t *pos,
                                         size_t max_size,
                                         uint32_t *state,
                                         int is_rle24)
{
    unsigned int absolute_pixels;
    size_t data_bytes;

    if (!fuzz_bmp_rle_mutator_append_u8(data, pos, max_size, 10u) ||
        !fuzz_bmp_rle_mutator_append_u8(data, pos, max_size, 0u) ||
        !fuzz_bmp_rle_mutator_append_u8(data, pos, max_size, 7u)) {
        return 0;
    }
    if (is_rle24 != 0) {
        if (!fuzz_bmp_rle_mutator_append_noise(data,
                                               pos,
                                               max_size,
                                               state,
                                               3u)) {
            return 0;
        }
    } else if (!fuzz_bmp_rle_mutator_append_u8(data, pos, max_size, 7u)) {
        return 0;
    }

    absolute_pixels = is_rle24 != 0 ? 2u : 3u;
    data_bytes = is_rle24 != 0 ? 6u : (size_t)absolute_pixels;
    if (!fuzz_bmp_rle_mutator_append_u8(data, pos, max_size, 1u) ||
        !fuzz_bmp_rle_mutator_append_u8(
            data,
            pos,
            max_size,
            (unsigned char)(absolute_pixels - 1u)) ||
        !fuzz_bmp_rle_mutator_append_noise(data,
                                           pos,
                                           max_size,
                                           state,
                                           data_bytes)) {
        return 0;
    }
    if ((data_bytes & 0x01u) != 0u &&
        !fuzz_bmp_rle_mutator_append_u8(data, pos, max_size, 0u)) {
        return 0;
    }

    return fuzz_bmp_rle_mutator_append_u8(data, pos, max_size, 2u) &&
           fuzz_bmp_rle_mutator_append_u8(data, pos, max_size, 3u) &&
           fuzz_bmp_rle_mutator_append_noise(data,
                                             pos,
                                             max_size,
                                             state,
                                             2u) &&
           fuzz_bmp_rle_mutator_append_u8(data, pos, max_size, 5u) &&
           fuzz_bmp_rle_mutator_append_noise(data,
                                             pos,
                                             max_size,
                                             state,
                                             3u) &&
           fuzz_bmp_rle_mutator_append_u8(data, pos, max_size, 4u);
}

static size_t
fuzz_bmp_rle_mutator_synthesize(uint8_t *data,
                                size_t max_size,
                                uint32_t *state)
{
    size_t pos;
    unsigned char mode;
    size_t palette_entries;
    int is_rle_mode;
    int is_rle24;

    if (data == NULL || state == NULL || max_size == 0u) {
        return 0u;
    }

    pos = 0u;
    mode = (unsigned char)(fuzz_bmp_rle_mutator_next(state) % 8u);
    palette_entries = 0u;
    is_rle_mode = 0;
    is_rle24 = 0;

    if (!fuzz_bmp_rle_mutator_append_u8(data, &pos, max_size, mode) ||
        !fuzz_bmp_rle_mutator_append_u8(data, &pos, max_size, 31u) ||
        !fuzz_bmp_rle_mutator_append_u8(data, &pos, max_size, 31u)) {
        return pos;
    }

    switch (mode) {
    case 0u:
    case 5u:
        palette_entries = 4u;
        is_rle_mode = 1;
        if (!fuzz_bmp_rle_mutator_append_u8(data, &pos, max_size, 3u)) {
            return pos;
        }
        break;
    case 1u:
    case 6u:
        palette_entries = 4u;
        is_rle_mode = 1;
        if (!fuzz_bmp_rle_mutator_append_u8(data, &pos, max_size, 3u)) {
            return pos;
        }
        break;
    case 2u:
        if (!fuzz_bmp_rle_mutator_append_u8(data, &pos, max_size, 1u) ||
            !fuzz_bmp_rle_mutator_append_u8(data, &pos, max_size, 1u) ||
            !fuzz_bmp_rle_mutator_append_u8(data, &pos, max_size, 1u)) {
            return pos;
        }
        break;
    case 4u:
        is_rle24 = 1;
        break;
    default:
        break;
    }

    if (!fuzz_bmp_rle_mutator_append_noise(data,
                                           &pos,
                                           max_size,
                                           state,
                                           16u + palette_entries * 4u)) {
        return pos;
    }
    if (mode == 2u &&
        !fuzz_bmp_rle_mutator_append_noise(data,
                                           &pos,
                                           max_size,
                                           state,
                                           17u)) {
        return pos;
    }

    if (is_rle_mode != 0 || is_rle24 != 0) {
        if (!fuzz_bmp_rle_mutator_append_rle_commands(data,
                                                      &pos,
                                                      max_size,
                                                      state,
                                                      is_rle24)) {
            return pos;
        }
    } else if (mode == 7u) {
        if (!fuzz_bmp_rle_mutator_append_u8(data, &pos, max_size, 0u) ||
            !fuzz_bmp_rle_mutator_append_u8(data, &pos, max_size, 16u) ||
            !fuzz_bmp_rle_mutator_append_noise(data,
                                               &pos,
                                               max_size,
                                               state,
                                               16u)) {
            return pos;
        }
    }

    (void)fuzz_bmp_rle_mutator_append_u8(data, &pos, max_size, 0u);
    return pos;
}

size_t
LLVMFuzzerCustomMutator(uint8_t *data,
                        size_t size,
                        size_t max_size,
                        unsigned int seed)
{
    uint32_t state;
    size_t mutated_size;

    if (data == NULL || max_size == 0u) {
        return 0u;
    }

    state = (uint32_t)seed ^ (uint32_t)size ^ UINT32_C(0x9e3779b9);
    if (size != 0u && (fuzz_bmp_rle_mutator_next(&state) & 0x07u) == 0u) {
        mutated_size = LLVMFuzzerMutate(data, size, max_size);
        if (mutated_size != 0u) {
            return mutated_size;
        }
    }

    return fuzz_bmp_rle_mutator_synthesize(data, max_size, &state);
}

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

            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_ENCODED_RUN);
            run_length =
                (unsigned char)(1u +
                                (fuzz_cursor_take_u8(cursor, 0u) % 255u));
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

            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_ABSOLUTE_RUN);
            absolute_pixels =
                (unsigned char)(1u +
                                (fuzz_cursor_take_u8(cursor, 0u) % 64u));
            data_bytes = is_rle4 != 0
                ? (size_t)((absolute_pixels + 1u) / 2u)
                : (size_t)absolute_pixels;

            if (!fuzz_byte_buffer_append_u8(stream, 0u) ||
                !fuzz_byte_buffer_append_u8(stream, absolute_pixels)) {
                return 0;
            }

            for (j = 0u; j < data_bytes; ++j) {
                if (!fuzz_byte_buffer_append_u8(
                        stream,
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
            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_EOL);
            if (!fuzz_byte_buffer_append_u8(stream, 0u) ||
                !fuzz_byte_buffer_append_u8(stream, 0u)) {
                return 0;
            }
            break;
        case 3u:
            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_DELTA);
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
            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_EOB);
            if (!fuzz_byte_buffer_append_u8(stream, 0u) ||
                !fuzz_byte_buffer_append_u8(stream, 1u)) {
                return 0;
            }
            return 1;
        default:
            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_ESCAPE);
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
fuzz_build_bmp_rle24_stream(fuzz_cursor_t *cursor,
                            fuzz_byte_buffer_t *stream)
{
    size_t command_count;
    size_t i;
    unsigned char mode;
    unsigned char run_length;
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char absolute_pixels;
    size_t data_bytes;
    size_t j;

    if (cursor == NULL || stream == NULL) {
        return 0;
    }

    command_count = 2u + (size_t)(fuzz_cursor_take_u8(cursor, 0u) % 96u);
    for (i = 0u; i < command_count; ++i) {
        mode = (unsigned char)(fuzz_cursor_take_u8(cursor, 0u) % 6u);
        switch (mode) {
        case 0u:
            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_ENCODED_RUN);
            run_length =
                (unsigned char)(1u +
                                (fuzz_cursor_take_u8(cursor, 0u) % 127u));
            blue = fuzz_cursor_take_u8(cursor, 0u);
            green = fuzz_cursor_take_u8(cursor, 0u);
            red = fuzz_cursor_take_u8(cursor, 0u);
            if (!fuzz_byte_buffer_append_u8(stream, run_length) ||
                !fuzz_byte_buffer_append_u8(stream, blue) ||
                !fuzz_byte_buffer_append_u8(stream, green) ||
                !fuzz_byte_buffer_append_u8(stream, red)) {
                return 0;
            }
            break;
        case 1u:
            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_ABSOLUTE_RUN);
            absolute_pixels =
                (unsigned char)(1u +
                                (fuzz_cursor_take_u8(cursor, 0u) % 32u));
            data_bytes = (size_t)absolute_pixels * 3u;
            if (!fuzz_byte_buffer_append_u8(stream, 0u) ||
                !fuzz_byte_buffer_append_u8(stream, absolute_pixels)) {
                return 0;
            }
            for (j = 0u; j < data_bytes; ++j) {
                if (!fuzz_byte_buffer_append_u8(
                        stream,
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
        case 2u:
            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_EOL);
            if (!fuzz_byte_buffer_append_u8(stream, 0u) ||
                !fuzz_byte_buffer_append_u8(stream, 0u)) {
                return 0;
            }
            break;
        case 3u:
            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_DELTA);
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
            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_EOB);
            if (!fuzz_byte_buffer_append_u8(stream, 0u) ||
                !fuzz_byte_buffer_append_u8(stream, 1u)) {
                return 0;
            }
            return 1;
        default:
            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_ESCAPE);
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
fuzz_build_bmp_huffman1d_stream(fuzz_cursor_t *cursor,
                                fuzz_byte_buffer_t *stream)
{
    static unsigned char const seed[6] = {
        0x35u, 0xc0u, 0x04u, 0x74u, 0x00u, 0x20u
    };
    size_t i;
    unsigned char byte;
    unsigned char mutate;

    if (cursor == NULL || stream == NULL) {
        return 0;
    }

    for (i = 0u; i < sizeof(seed); ++i) {
        byte = seed[i];
        mutate = fuzz_cursor_take_u8(cursor, 0u);
        if ((mutate & 0x07u) == 0u) {
            byte ^= fuzz_cursor_take_u8(cursor, 0u);
        }
        if (!fuzz_byte_buffer_append_u8(stream, byte)) {
            return 0;
        }
    }

    return 1;
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

    fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_MASKS);
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
    enum {
        FUZZ_BMP_MODE_RLE8 = 0u,
        FUZZ_BMP_MODE_RLE4 = 1u,
        FUZZ_BMP_MODE_BITFIELDS = 2u,
        FUZZ_BMP_MODE_OS2_HUFFMAN1D = 3u,
        FUZZ_BMP_MODE_OS2_RLE24 = 4u,
        FUZZ_BMP_MODE_CMYKRLE8 = 5u,
        FUZZ_BMP_MODE_CMYKRLE4 = 6u,
        FUZZ_BMP_MODE_CMYK_RAW = 7u
    };

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
    int is_cmyk_palette;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    fuzz_byte_buffer_init(&pixel_data);

    mode = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 8u);
    width = 1u + (uint32_t)(fuzz_cursor_take_u8(&cursor, 1u) % 128u);
    height = 1u + (uint32_t)(fuzz_cursor_take_u8(&cursor, 1u) % 128u);
    dib_size = 40u;
    is_cmyk_palette = 0;

    switch (mode) {
    case FUZZ_BMP_MODE_RLE8:
        fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_RLE8);
        bits_per_pixel = 8u;
        compression = 1u;
        palette_entries =
            1u + (size_t)(fuzz_cursor_take_u8(&cursor, 0u) % 256u);
        break;
    case FUZZ_BMP_MODE_RLE4:
        fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_RLE4);
        bits_per_pixel = 4u;
        compression = 2u;
        palette_entries = 1u + (size_t)(fuzz_cursor_take_u8(&cursor, 0u) %
                                        16u);
        break;
    case FUZZ_BMP_MODE_BITFIELDS:
        fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_BITFIELDS);
        bits_per_pixel =
            (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u ? 16u : 32u;
        compression =
            (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u ? 6u : 3u;
        if (compression == 6u) {
            fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_ALPHA_BITFIELDS);
        }
        palette_entries = (fuzz_cursor_take_u8(&cursor, 0u) & 0x07u) == 0u
            ? (size_t)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 8u))
            : 0u;
        break;
    case FUZZ_BMP_MODE_OS2_HUFFMAN1D:
        fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_OS2_HUFFMAN1D);
        dib_size = 64u;
        width = 2u;
        height = 2u;
        bits_per_pixel = 1u;
        compression = 3u;
        palette_entries = 2u;
        break;
    case FUZZ_BMP_MODE_OS2_RLE24:
        fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_OS2_RLE24);
        dib_size = 64u;
        bits_per_pixel = 24u;
        compression = 4u;
        palette_entries = 0u;
        break;
    case FUZZ_BMP_MODE_CMYKRLE8:
        fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_CMYK_RLE);
        bits_per_pixel = 8u;
        compression = 12u;
        palette_entries =
            1u + (size_t)(fuzz_cursor_take_u8(&cursor, 0u) % 256u);
        is_cmyk_palette = 1;
        break;
    case FUZZ_BMP_MODE_CMYKRLE4:
        fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_CMYK_RLE);
        bits_per_pixel = 4u;
        compression = 13u;
        palette_entries = 1u + (size_t)(fuzz_cursor_take_u8(&cursor, 0u) %
                                        16u);
        is_cmyk_palette = 1;
        break;
    case FUZZ_BMP_MODE_CMYK_RAW:
    default:
        fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_CMYK_RAW);
        bits_per_pixel = 32u;
        compression = 11u;
        palette_entries = 0u;
        is_cmyk_palette = 1;
        break;
    }

    colors_used = (uint32_t)palette_entries;
    if (palette_entries > 0u) {
        fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_PALETTE);
    }
    pixel_offset = 14u + (size_t)dib_size + palette_entries * 4u;
    if (compression == 3u && dib_size == 40u) {
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
                                       (uint32_t)
                                           fuzz_cursor_take_u32be(&cursor,
                                                                  0u)) ||
        !fuzz_byte_buffer_append_u32le(payload,
                                       (uint32_t)
                                           fuzz_cursor_take_u32be(&cursor,
                                                                  2835u)) ||
        !fuzz_byte_buffer_append_u32le(payload,
                                       (uint32_t)
                                           fuzz_cursor_take_u32be(&cursor,
                                                                  2835u)) ||
        !fuzz_byte_buffer_append_u32le(payload, colors_used) ||
        !fuzz_byte_buffer_append_u32le(payload,
                                       (uint32_t)
                                           fuzz_cursor_take_u32be(&cursor,
                                                                  0u))) {
        fuzz_byte_buffer_reset(&pixel_data);
        return 0;
    }
    fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_HEADER);

    if ((compression == 3u && dib_size == 40u) || compression == 6u) {
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
            !fuzz_byte_buffer_append_u8(
                payload,
                is_cmyk_palette != 0
                    ? fuzz_cursor_take_u8(&cursor, 0u)
                    : 0u)) {
            fuzz_byte_buffer_reset(&pixel_data);
            return 0;
        }
    }

    if (compression == 1u || compression == 2u ||
        compression == 12u || compression == 13u) {
        if (!fuzz_build_bmp_rle_stream(&cursor,
                                       &pixel_data,
                                       compression == 2u ||
                                       compression == 13u ? 1 : 0)) {
            fuzz_byte_buffer_reset(&pixel_data);
            return 0;
        }
    } else if (compression == 4u) {
        if (!fuzz_build_bmp_rle24_stream(&cursor, &pixel_data)) {
            fuzz_byte_buffer_reset(&pixel_data);
            return 0;
        }
    } else if (compression == 3u && dib_size == 64u) {
        if (!fuzz_build_bmp_huffman1d_stream(&cursor, &pixel_data)) {
            fuzz_byte_buffer_reset(&pixel_data);
            return 0;
        }
    } else {
        size_t raw_size;

        fuzz_bmp_rle_note_feature(FUZZ_BMP_RLE_FEATURE_RAW_DATA);
        raw_size =
            (size_t)(32u +
                     (fuzz_cursor_take_u16be(&cursor, 0u) % 8193u));
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
    fuzz_bmp_rle_register_semantic_stats();
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

    fuzz_bmp_rle_register_semantic_stats();
    fuzz_byte_buffer_init(&payload);
    if (!fuzz_build_bmp_rle_bitfields_payload(data, size, &payload)) {
        fuzz_byte_buffer_reset(&payload);
        return 0;
    }
    if (!fuzz_loader_builtin_runtime_bootstrap()) {
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
