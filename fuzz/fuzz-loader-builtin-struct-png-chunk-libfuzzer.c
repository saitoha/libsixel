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

enum fuzz_png_chunk_feature {
    FUZZ_PNG_CHUNK_FEATURE_SIGNATURE = 0,
    FUZZ_PNG_CHUNK_FEATURE_IHDR,
    FUZZ_PNG_CHUNK_FEATURE_IDAT,
    FUZZ_PNG_CHUNK_FEATURE_PLTE,
    FUZZ_PNG_CHUNK_FEATURE_TRNS,
    FUZZ_PNG_CHUNK_FEATURE_ICCP,
    FUZZ_PNG_CHUNK_FEATURE_GAMA,
    FUZZ_PNG_CHUNK_FEATURE_TEXT,
    FUZZ_PNG_CHUNK_FEATURE_ANCILLARY,
    FUZZ_PNG_CHUNK_FEATURE_BAD_CRC,
    FUZZ_PNG_CHUNK_FEATURE_FALLBACK_IDAT,
    FUZZ_PNG_CHUNK_FEATURE_IEND,
    FUZZ_PNG_CHUNK_FEATURE_COUNT
};

static unsigned int g_png_chunk_feature_mask = 0u;
static unsigned int g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_COUNT];
static int g_png_chunk_stats_registered = 0;

#define FUZZ_PNG_CHUNK_RECORD_FEATURE(_feature) \
    do { \
        g_png_chunk_feature_mask |= 1u << (_feature); \
        ++g_png_chunk_feature_hits[(_feature)]; \
    } while (0)

static unsigned int
fuzz_png_chunk_count_bits(unsigned int value)
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
fuzz_png_chunk_note_feature(unsigned int feature)
{
    if (feature >= FUZZ_PNG_CHUNK_FEATURE_COUNT) {
        return;
    }

    /*
     * PNG ancillary chunks share parser edges with unrelated chunk classes.
     * These milestones keep chunk families visible to coverage-guided fuzzing.
     */
    switch (feature) {
    case FUZZ_PNG_CHUNK_FEATURE_SIGNATURE:
        FUZZ_PNG_CHUNK_RECORD_FEATURE(FUZZ_PNG_CHUNK_FEATURE_SIGNATURE);
        break;
    case FUZZ_PNG_CHUNK_FEATURE_IHDR:
        FUZZ_PNG_CHUNK_RECORD_FEATURE(FUZZ_PNG_CHUNK_FEATURE_IHDR);
        break;
    case FUZZ_PNG_CHUNK_FEATURE_IDAT:
        FUZZ_PNG_CHUNK_RECORD_FEATURE(FUZZ_PNG_CHUNK_FEATURE_IDAT);
        break;
    case FUZZ_PNG_CHUNK_FEATURE_PLTE:
        FUZZ_PNG_CHUNK_RECORD_FEATURE(FUZZ_PNG_CHUNK_FEATURE_PLTE);
        break;
    case FUZZ_PNG_CHUNK_FEATURE_TRNS:
        FUZZ_PNG_CHUNK_RECORD_FEATURE(FUZZ_PNG_CHUNK_FEATURE_TRNS);
        break;
    case FUZZ_PNG_CHUNK_FEATURE_ICCP:
        FUZZ_PNG_CHUNK_RECORD_FEATURE(FUZZ_PNG_CHUNK_FEATURE_ICCP);
        break;
    case FUZZ_PNG_CHUNK_FEATURE_GAMA:
        FUZZ_PNG_CHUNK_RECORD_FEATURE(FUZZ_PNG_CHUNK_FEATURE_GAMA);
        break;
    case FUZZ_PNG_CHUNK_FEATURE_TEXT:
        FUZZ_PNG_CHUNK_RECORD_FEATURE(FUZZ_PNG_CHUNK_FEATURE_TEXT);
        break;
    case FUZZ_PNG_CHUNK_FEATURE_ANCILLARY:
        FUZZ_PNG_CHUNK_RECORD_FEATURE(FUZZ_PNG_CHUNK_FEATURE_ANCILLARY);
        break;
    case FUZZ_PNG_CHUNK_FEATURE_BAD_CRC:
        FUZZ_PNG_CHUNK_RECORD_FEATURE(FUZZ_PNG_CHUNK_FEATURE_BAD_CRC);
        break;
    case FUZZ_PNG_CHUNK_FEATURE_FALLBACK_IDAT:
        FUZZ_PNG_CHUNK_RECORD_FEATURE(FUZZ_PNG_CHUNK_FEATURE_FALLBACK_IDAT);
        break;
    case FUZZ_PNG_CHUNK_FEATURE_IEND:
        FUZZ_PNG_CHUNK_RECORD_FEATURE(FUZZ_PNG_CHUNK_FEATURE_IEND);
        break;
    default:
        break;
    }
}

static void
fuzz_png_chunk_print_semantic_stats(void)
{
    fprintf(stderr,
            "png-chunk-semantic-features: mask=0x%08x count=%u "
            "signature=%u ihdr=%u idat=%u plte=%u trns=%u iccp=%u "
            "gama=%u text=%u ancillary=%u bad-crc=%u "
            "fallback-idat=%u iend=%u\n",
            g_png_chunk_feature_mask,
            fuzz_png_chunk_count_bits(g_png_chunk_feature_mask),
            g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_SIGNATURE],
            g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_IHDR],
            g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_IDAT],
            g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_PLTE],
            g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_TRNS],
            g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_ICCP],
            g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_GAMA],
            g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_TEXT],
            g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_ANCILLARY],
            g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_BAD_CRC],
            g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_FALLBACK_IDAT],
            g_png_chunk_feature_hits[FUZZ_PNG_CHUNK_FEATURE_IEND]);
}

static void
fuzz_png_chunk_register_semantic_stats(void)
{
    if (g_png_chunk_stats_registered) {
        return;
    }

    (void)atexit(fuzz_png_chunk_print_semantic_stats);
    g_png_chunk_stats_registered = 1;
}

static int
fuzz_png_chunk_type_is(char const type[4],
                       char a,
                       char b,
                       char c,
                       char d)
{
    return type[0] == a && type[1] == b &&
           type[2] == c && type[3] == d;
}

static int
fuzz_png_chunk_append_chunk(fuzz_byte_buffer_t *payload,
                            char const type[4],
                            unsigned char const *data,
                            size_t length,
                            int force_bad_crc,
                            uint32_t crc_salt)
{
    if (force_bad_crc != 0) {
        fuzz_png_chunk_note_feature(FUZZ_PNG_CHUNK_FEATURE_BAD_CRC);
    }
    if ((type[0] & 0x20) != 0) {
        fuzz_png_chunk_note_feature(FUZZ_PNG_CHUNK_FEATURE_ANCILLARY);
    }

    if (fuzz_png_chunk_type_is(type, 'I', 'H', 'D', 'R')) {
        fuzz_png_chunk_note_feature(FUZZ_PNG_CHUNK_FEATURE_IHDR);
    } else if (fuzz_png_chunk_type_is(type, 'I', 'D', 'A', 'T')) {
        fuzz_png_chunk_note_feature(FUZZ_PNG_CHUNK_FEATURE_IDAT);
    } else if (fuzz_png_chunk_type_is(type, 'P', 'L', 'T', 'E')) {
        fuzz_png_chunk_note_feature(FUZZ_PNG_CHUNK_FEATURE_PLTE);
    } else if (fuzz_png_chunk_type_is(type, 't', 'R', 'N', 'S')) {
        fuzz_png_chunk_note_feature(FUZZ_PNG_CHUNK_FEATURE_TRNS);
    } else if (fuzz_png_chunk_type_is(type, 'i', 'C', 'C', 'P')) {
        fuzz_png_chunk_note_feature(FUZZ_PNG_CHUNK_FEATURE_ICCP);
    } else if (fuzz_png_chunk_type_is(type, 'g', 'A', 'M', 'A')) {
        fuzz_png_chunk_note_feature(FUZZ_PNG_CHUNK_FEATURE_GAMA);
    } else if (fuzz_png_chunk_type_is(type, 't', 'E', 'X', 't') ||
               fuzz_png_chunk_type_is(type, 'i', 'T', 'X', 't') ||
               fuzz_png_chunk_type_is(type, 'z', 'T', 'X', 't')) {
        fuzz_png_chunk_note_feature(FUZZ_PNG_CHUNK_FEATURE_TEXT);
    } else if (fuzz_png_chunk_type_is(type, 'I', 'E', 'N', 'D')) {
        fuzz_png_chunk_note_feature(FUZZ_PNG_CHUNK_FEATURE_IEND);
    }

    return fuzz_append_png_chunk(payload,
                                 type,
                                 data,
                                 length,
                                 force_bad_crc,
                                 crc_salt);
}

static uint32_t
fuzz_png_chunk_mutator_next(uint32_t *state)
{
    uint32_t value;

    value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    if (value == 0u) {
        value = UINT32_C(0xa24baed5);
    }
    *state = value;

    return value;
}

static int
fuzz_png_chunk_mutator_append_u8(uint8_t *data,
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
fuzz_png_chunk_mutator_append_u16(uint8_t *data,
                                  size_t *pos,
                                  size_t max_size,
                                  uint16_t value)
{
    return fuzz_png_chunk_mutator_append_u8(
        data,
        pos,
        max_size,
        (unsigned char)((value >> 8) & 0xffu)) &&
        fuzz_png_chunk_mutator_append_u8(
            data,
            pos,
            max_size,
            (unsigned char)(value & 0xffu));
}

static int
fuzz_png_chunk_mutator_append_u32(uint8_t *data,
                                  size_t *pos,
                                  size_t max_size,
                                  uint32_t value)
{
    return fuzz_png_chunk_mutator_append_u8(
        data,
        pos,
        max_size,
        (unsigned char)((value >> 24) & 0xffu)) &&
        fuzz_png_chunk_mutator_append_u8(
            data,
            pos,
            max_size,
            (unsigned char)((value >> 16) & 0xffu)) &&
        fuzz_png_chunk_mutator_append_u8(
            data,
            pos,
            max_size,
            (unsigned char)((value >> 8) & 0xffu)) &&
        fuzz_png_chunk_mutator_append_u8(
            data,
            pos,
            max_size,
            (unsigned char)(value & 0xffu));
}

static int
fuzz_png_chunk_mutator_append_span(uint8_t *data,
                                   size_t *pos,
                                   size_t max_size,
                                   uint8_t const *source,
                                   size_t source_size)
{
    size_t copy_size;

    if (data == NULL || pos == NULL || source == NULL || *pos >= max_size) {
        return 0;
    }

    copy_size = source_size;
    if (copy_size > max_size - *pos) {
        copy_size = max_size - *pos;
    }
    if (copy_size == 0u) {
        return 1;
    }

    memcpy(data + *pos, source, copy_size);
    *pos += copy_size;
    return 1;
}

static unsigned int
fuzz_png_chunk_mutator_choose_focus(uint32_t *state)
{
    unsigned int feature;
    unsigned int best;
    unsigned int hits;
    unsigned int best_hits;

    if (state == NULL) {
        return FUZZ_PNG_CHUNK_FEATURE_SIGNATURE;
    }

    best = (unsigned int)(fuzz_png_chunk_mutator_next(state) %
                          FUZZ_PNG_CHUNK_FEATURE_COUNT);
    best_hits = g_png_chunk_feature_hits[best];
    for (feature = 0u; feature < FUZZ_PNG_CHUNK_FEATURE_COUNT; ++feature) {
        hits = g_png_chunk_feature_hits[feature];
        if (hits < best_hits ||
            (hits == best_hits &&
             (fuzz_png_chunk_mutator_next(state) & 0x01u) != 0u)) {
            best = feature;
            best_hits = hits;
        }
    }

    return best;
}

static int
fuzz_png_chunk_mutator_append_chunk_ctl(uint8_t *data,
                                        size_t *pos,
                                        size_t max_size,
                                        uint32_t *state,
                                        unsigned char type_index,
                                        int bad_crc)
{
    unsigned char payload_size;
    unsigned char i;

    payload_size = type_index == 1u ? 6u : 0u;
    if (type_index == 7u || type_index == 10u) {
        payload_size = 8u;
    }

    if (!fuzz_png_chunk_mutator_append_u8(data, pos, max_size, 0u) ||
        !fuzz_png_chunk_mutator_append_u8(data,
                                          pos,
                                          max_size,
                                          type_index) ||
        !fuzz_png_chunk_mutator_append_u16(data,
                                           pos,
                                           max_size,
                                           payload_size)) {
        return 0;
    }

    for (i = 0u; i < payload_size; ++i) {
        if (!fuzz_png_chunk_mutator_append_u8(
                data,
                pos,
                max_size,
                (unsigned char)fuzz_png_chunk_mutator_next(state))) {
            return 0;
        }
    }

    return fuzz_png_chunk_mutator_append_u8(
        data,
        pos,
        max_size,
        bad_crc != 0 ? 1u : 0u) &&
        fuzz_png_chunk_mutator_append_u32(
            data,
            pos,
            max_size,
            fuzz_png_chunk_mutator_next(state));
}

static size_t
fuzz_png_chunk_mutator_synthesize(uint8_t *data,
                                  size_t max_size,
                                  uint32_t *state,
                                  unsigned int focus)
{
    size_t pos;
    int bad_crc;

    if (data == NULL || state == NULL || max_size == 0u) {
        return 0u;
    }

    pos = 0u;
    bad_crc = focus == FUZZ_PNG_CHUNK_FEATURE_BAD_CRC ? 1 : 0;
    if (!fuzz_png_chunk_mutator_append_u16(data, &pos, max_size, 64u) ||
        !fuzz_png_chunk_mutator_append_u16(data, &pos, max_size, 64u) ||
        !fuzz_png_chunk_mutator_append_u8(data, &pos, max_size, 3u) ||
        !fuzz_png_chunk_mutator_append_u8(data, &pos, max_size, 2u) ||
        !fuzz_png_chunk_mutator_append_u8(data, &pos, max_size, 0u) ||
        !fuzz_png_chunk_mutator_append_u8(data,
                                          &pos,
                                          max_size,
                                          bad_crc != 0 ? 1u : 0u) ||
        !fuzz_png_chunk_mutator_append_u32(data,
                                           &pos,
                                           max_size,
                                           fuzz_png_chunk_mutator_next(
                                               state)) ||
        !fuzz_png_chunk_mutator_append_u8(data, &pos, max_size, 5u)) {
        return pos;
    }

    if (focus == FUZZ_PNG_CHUNK_FEATURE_IDAT) {
        (void)fuzz_png_chunk_mutator_append_chunk_ctl(data,
                                                      &pos,
                                                      max_size,
                                                      state,
                                                      0u,
                                                      0);
    } else {
        (void)fuzz_png_chunk_mutator_append_chunk_ctl(data,
                                                      &pos,
                                                      max_size,
                                                      state,
                                                      1u,
                                                      0);
    }
    (void)fuzz_png_chunk_mutator_append_chunk_ctl(data,
                                                  &pos,
                                                  max_size,
                                                  state,
                                                  2u,
                                                  0);
    (void)fuzz_png_chunk_mutator_append_chunk_ctl(data,
                                                  &pos,
                                                  max_size,
                                                  state,
                                                  3u,
                                                  0);
    (void)fuzz_png_chunk_mutator_append_chunk_ctl(data,
                                                  &pos,
                                                  max_size,
                                                  state,
                                                  7u,
                                                  0);
    (void)fuzz_png_chunk_mutator_append_chunk_ctl(data,
                                                  &pos,
                                                  max_size,
                                                  state,
                                                  10u,
                                                  bad_crc);
    (void)fuzz_png_chunk_mutator_append_chunk_ctl(data,
                                                  &pos,
                                                  max_size,
                                                  state,
                                                  11u,
                                                  0);

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
    unsigned int focus;

    if (data == NULL || max_size == 0u) {
        return 0u;
    }

    state = (uint32_t)seed ^ (uint32_t)size ^ UINT32_C(0x9e3779b1);
    focus = fuzz_png_chunk_mutator_choose_focus(&state);
    if (size != 0u && (fuzz_png_chunk_mutator_next(&state) & 0x07u) == 0u) {
        mutated_size = LLVMFuzzerMutate(data, size, max_size);
        if (mutated_size != 0u) {
            return mutated_size;
        }
    }

    return fuzz_png_chunk_mutator_synthesize(data, max_size, &state, focus);
}

size_t
LLVMFuzzerCustomCrossOver(uint8_t const *data1,
                          size_t size1,
                          uint8_t const *data2,
                          size_t size2,
                          uint8_t *out,
                          size_t max_out_size,
                          unsigned int seed)
{
    uint32_t state;
    uint8_t const *prefix_data;
    uint8_t const *suffix_data;
    size_t prefix_data_size;
    size_t suffix_data_size;
    size_t prefix_size;
    size_t suffix_offset;
    size_t pos;
    unsigned int focus;

    if (out == NULL || max_out_size == 0u) {
        return 0u;
    }

    state = (uint32_t)seed ^ (uint32_t)size1 ^
            ((uint32_t)size2 << 1) ^ UINT32_C(0x1b873593);
    focus = fuzz_png_chunk_mutator_choose_focus(&state);
    if ((fuzz_png_chunk_mutator_next(&state) & 0x01u) == 0u) {
        prefix_data = data1;
        prefix_data_size = size1;
        suffix_data = data2;
        suffix_data_size = size2;
    } else {
        prefix_data = data2;
        prefix_data_size = size2;
        suffix_data = data1;
        suffix_data_size = size1;
    }

    pos = 0u;
    prefix_size = prefix_data_size < 13u ? prefix_data_size : 13u;
    if (prefix_data != NULL && prefix_size > 0u) {
        (void)fuzz_png_chunk_mutator_append_span(out,
                                                 &pos,
                                                 max_out_size,
                                                 prefix_data,
                                                 prefix_size);
    }

    suffix_offset = suffix_data_size < 13u ? suffix_data_size : 13u;
    if (suffix_data != NULL && suffix_offset < suffix_data_size) {
        (void)fuzz_png_chunk_mutator_append_span(
            out,
            &pos,
            max_out_size,
            suffix_data + suffix_offset,
            suffix_data_size - suffix_offset);
    }
    if (pos == 0u) {
        return fuzz_png_chunk_mutator_synthesize(out,
                                                 max_out_size,
                                                 &state,
                                                 focus);
    }

    return pos;
}

static int
fuzz_build_png_chunk_payload(uint8_t const *data,
                             size_t size,
                             fuzz_byte_buffer_t *payload)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    static char const chunk_types[][4] = {
        { 'I', 'D', 'A', 'T' },
        { 'P', 'L', 'T', 'E' },
        { 't', 'R', 'N', 'S' },
        { 'g', 'A', 'M', 'A' },
        { 'c', 'H', 'R', 'M' },
        { 's', 'R', 'G', 'B' },
        { 'p', 'H', 'Y', 's' },
        { 't', 'E', 'X', 't' },
        { 'i', 'T', 'X', 't' },
        { 'z', 'T', 'X', 't' },
        { 'i', 'C', 'C', 'P' },
        { 'b', 'K', 'G', 'D' },
        { 's', 'B', 'I', 'T' }
    };

    fuzz_cursor_t cursor;
    unsigned char ihdr[13];
    uint32_t width;
    uint32_t height;
    unsigned char bit_depths[5];
    unsigned char color_types[5];
    size_t chunk_count;
    size_t i;
    int saw_idat;

    if (payload == NULL) {
        return 0;
    }

    bit_depths[0] = 1u;
    bit_depths[1] = 2u;
    bit_depths[2] = 4u;
    bit_depths[3] = 8u;
    bit_depths[4] = 16u;

    color_types[0] = 0u;
    color_types[1] = 2u;
    color_types[2] = 3u;
    color_types[3] = 4u;
    color_types[4] = 6u;

    fuzz_cursor_init(&cursor, data, size);
    if (!fuzz_byte_buffer_append(payload,
                                 png_signature,
                                 sizeof(png_signature))) {
        return 0;
    }
    fuzz_png_chunk_note_feature(FUZZ_PNG_CHUNK_FEATURE_SIGNATURE);

    width = 1u + (uint32_t)(fuzz_cursor_take_u16be(&cursor, 1u) % 1024u);
    height = 1u + (uint32_t)(fuzz_cursor_take_u16be(&cursor, 1u) % 1024u);

    ihdr[0] = (unsigned char)((width >> 24) & 0xffu);
    ihdr[1] = (unsigned char)((width >> 16) & 0xffu);
    ihdr[2] = (unsigned char)((width >> 8) & 0xffu);
    ihdr[3] = (unsigned char)(width & 0xffu);
    ihdr[4] = (unsigned char)((height >> 24) & 0xffu);
    ihdr[5] = (unsigned char)((height >> 16) & 0xffu);
    ihdr[6] = (unsigned char)((height >> 8) & 0xffu);
    ihdr[7] = (unsigned char)(height & 0xffu);
    ihdr[8] = bit_depths[fuzz_cursor_take_u8(&cursor, 3u) % 5u];
    ihdr[9] = color_types[fuzz_cursor_take_u8(&cursor, 1u) % 5u];
    ihdr[10] = 0u;
    ihdr[11] = 0u;
    ihdr[12] = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u);

    if (!fuzz_png_chunk_append_chunk(
            payload,
            "IHDR",
            ihdr,
            sizeof(ihdr),
            (int)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u),
            (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u))) {
        return 0;
    }

    chunk_count = 1u + (size_t)(fuzz_cursor_take_u8(&cursor, 0u) % 6u);
    saw_idat = 0;
    for (i = 0u; i < chunk_count; ++i) {
        char chunk_type[4];
        unsigned char const *chunk_data;
        size_t chunk_length;

        chunk_data = NULL;
        chunk_length = 0u;
        if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u &&
            fuzz_cursor_remaining(&cursor) >= 4u) {
            chunk_type[0] = (char)fuzz_cursor_take_u8(&cursor, 'I');
            chunk_type[1] = (char)fuzz_cursor_take_u8(&cursor, 'D');
            chunk_type[2] = (char)fuzz_cursor_take_u8(&cursor, 'A');
            chunk_type[3] = (char)fuzz_cursor_take_u8(&cursor, 'T');
        } else {
            size_t type_index;

            type_index = (size_t)
                (fuzz_cursor_take_u8(&cursor, 0u) %
                 (sizeof(chunk_types) / sizeof(chunk_types[0])));
            chunk_type[0] = chunk_types[type_index][0];
            chunk_type[1] = chunk_types[type_index][1];
            chunk_type[2] = chunk_types[type_index][2];
            chunk_type[3] = chunk_types[type_index][3];
        }

        if (chunk_type[0] == 'I' &&
            chunk_type[1] == 'D' &&
            chunk_type[2] == 'A' &&
            chunk_type[3] == 'T') {
            saw_idat = 1;
        }

        if (fuzz_cursor_remaining(&cursor) > 0u) {
            chunk_length = (size_t)
                (fuzz_cursor_take_u16be(&cursor, 0u) % 1025u);
            if (chunk_length > fuzz_cursor_remaining(&cursor)) {
                chunk_length = fuzz_cursor_remaining(&cursor);
            }
            chunk_data = cursor.data + cursor.pos;
            cursor.pos += chunk_length;
        }

        if (!fuzz_png_chunk_append_chunk(
                payload,
                chunk_type,
                chunk_data,
                chunk_length,
                (int)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u),
                (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u))) {
            return 0;
        }
    }

    if (!saw_idat) {
        unsigned char const fallback_idat[8] = {
            0x78u, 0x9cu, 0x63u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u
        };
        fuzz_png_chunk_note_feature(FUZZ_PNG_CHUNK_FEATURE_FALLBACK_IDAT);
        if (!fuzz_png_chunk_append_chunk(payload,
                                         "IDAT",
                                         fallback_idat,
                                         sizeof(fallback_idat),
                                         0,
                                         0u)) {
            return 0;
        }
    }

    if (!fuzz_png_chunk_append_chunk(payload, "IEND", NULL, 0u, 0, 0u)) {
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
    fuzz_png_chunk_register_semantic_stats();
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
    fuzz_png_chunk_register_semantic_stats();

    fuzz_byte_buffer_init(&payload);
    if (!fuzz_build_png_chunk_payload(data, size, &payload)) {
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
