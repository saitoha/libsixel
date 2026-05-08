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

enum fuzz_png_apng_feature {
    FUZZ_PNG_APNG_FEATURE_SIGNATURE = 0,
    FUZZ_PNG_APNG_FEATURE_IHDR,
    FUZZ_PNG_APNG_FEATURE_ACTL,
    FUZZ_PNG_APNG_FEATURE_MULTI_FRAME,
    FUZZ_PNG_APNG_FEATURE_FCTL,
    FUZZ_PNG_APNG_FEATURE_IDAT,
    FUZZ_PNG_APNG_FEATURE_FDAT,
    FUZZ_PNG_APNG_FEATURE_SEQUENCE_SKEW,
    FUZZ_PNG_APNG_FEATURE_BAD_CRC,
    FUZZ_PNG_APNG_FEATURE_FALLBACK_IDAT,
    FUZZ_PNG_APNG_FEATURE_DISPOSE,
    FUZZ_PNG_APNG_FEATURE_BLEND,
    FUZZ_PNG_APNG_FEATURE_COUNT
};

static unsigned int g_png_apng_feature_mask = 0u;
static unsigned int g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_COUNT];
static int g_png_apng_stats_registered = 0;

#define FUZZ_PNG_APNG_RECORD_FEATURE(_feature) \
    do { \
        g_png_apng_feature_mask |= 1u << (_feature); \
        ++g_png_apng_feature_hits[(_feature)]; \
    } while (0)

static unsigned int
fuzz_png_apng_count_bits(unsigned int value)
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
fuzz_png_apng_note_feature(unsigned int feature)
{
    if (feature >= FUZZ_PNG_APNG_FEATURE_COUNT) {
        return;
    }

    /*
     * APNG coverage needs chunk-order milestones. A malformed sequence can
     * reuse the same parser edges while exercising different animation state.
     */
    switch (feature) {
    case FUZZ_PNG_APNG_FEATURE_SIGNATURE:
        FUZZ_PNG_APNG_RECORD_FEATURE(FUZZ_PNG_APNG_FEATURE_SIGNATURE);
        break;
    case FUZZ_PNG_APNG_FEATURE_IHDR:
        FUZZ_PNG_APNG_RECORD_FEATURE(FUZZ_PNG_APNG_FEATURE_IHDR);
        break;
    case FUZZ_PNG_APNG_FEATURE_ACTL:
        FUZZ_PNG_APNG_RECORD_FEATURE(FUZZ_PNG_APNG_FEATURE_ACTL);
        break;
    case FUZZ_PNG_APNG_FEATURE_MULTI_FRAME:
        FUZZ_PNG_APNG_RECORD_FEATURE(FUZZ_PNG_APNG_FEATURE_MULTI_FRAME);
        break;
    case FUZZ_PNG_APNG_FEATURE_FCTL:
        FUZZ_PNG_APNG_RECORD_FEATURE(FUZZ_PNG_APNG_FEATURE_FCTL);
        break;
    case FUZZ_PNG_APNG_FEATURE_IDAT:
        FUZZ_PNG_APNG_RECORD_FEATURE(FUZZ_PNG_APNG_FEATURE_IDAT);
        break;
    case FUZZ_PNG_APNG_FEATURE_FDAT:
        FUZZ_PNG_APNG_RECORD_FEATURE(FUZZ_PNG_APNG_FEATURE_FDAT);
        break;
    case FUZZ_PNG_APNG_FEATURE_SEQUENCE_SKEW:
        FUZZ_PNG_APNG_RECORD_FEATURE(FUZZ_PNG_APNG_FEATURE_SEQUENCE_SKEW);
        break;
    case FUZZ_PNG_APNG_FEATURE_BAD_CRC:
        FUZZ_PNG_APNG_RECORD_FEATURE(FUZZ_PNG_APNG_FEATURE_BAD_CRC);
        break;
    case FUZZ_PNG_APNG_FEATURE_FALLBACK_IDAT:
        FUZZ_PNG_APNG_RECORD_FEATURE(FUZZ_PNG_APNG_FEATURE_FALLBACK_IDAT);
        break;
    case FUZZ_PNG_APNG_FEATURE_DISPOSE:
        FUZZ_PNG_APNG_RECORD_FEATURE(FUZZ_PNG_APNG_FEATURE_DISPOSE);
        break;
    case FUZZ_PNG_APNG_FEATURE_BLEND:
        FUZZ_PNG_APNG_RECORD_FEATURE(FUZZ_PNG_APNG_FEATURE_BLEND);
        break;
    default:
        break;
    }
}

static void
fuzz_png_apng_print_semantic_stats(void)
{
    fprintf(stderr,
            "png-apng-semantic-features: mask=0x%08x count=%u "
            "signature=%u ihdr=%u actl=%u multi-frame=%u fctl=%u "
            "idat=%u fdat=%u sequence-skew=%u bad-crc=%u "
            "fallback-idat=%u dispose=%u blend=%u\n",
            g_png_apng_feature_mask,
            fuzz_png_apng_count_bits(g_png_apng_feature_mask),
            g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_SIGNATURE],
            g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_IHDR],
            g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_ACTL],
            g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_MULTI_FRAME],
            g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_FCTL],
            g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_IDAT],
            g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_FDAT],
            g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_SEQUENCE_SKEW],
            g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_BAD_CRC],
            g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_FALLBACK_IDAT],
            g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_DISPOSE],
            g_png_apng_feature_hits[FUZZ_PNG_APNG_FEATURE_BLEND]);
}

static void
fuzz_png_apng_register_semantic_stats(void)
{
    if (g_png_apng_stats_registered) {
        return;
    }

    (void)atexit(fuzz_png_apng_print_semantic_stats);
    g_png_apng_stats_registered = 1;
}

static uint32_t
fuzz_png_apng_mutator_next(uint32_t *state)
{
    uint32_t value;

    value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    if (value == 0u) {
        value = UINT32_C(0x85ebca6b);
    }
    *state = value;

    return value;
}

static int
fuzz_png_apng_mutator_append_u8(uint8_t *data,
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
fuzz_png_apng_mutator_append_u32(uint8_t *data,
                                 size_t *pos,
                                 size_t max_size,
                                 uint32_t value)
{
    return fuzz_png_apng_mutator_append_u8(
        data,
        pos,
        max_size,
        (unsigned char)((value >> 24) & 0xffu)) &&
        fuzz_png_apng_mutator_append_u8(
            data,
            pos,
            max_size,
            (unsigned char)((value >> 16) & 0xffu)) &&
        fuzz_png_apng_mutator_append_u8(
            data,
            pos,
            max_size,
            (unsigned char)((value >> 8) & 0xffu)) &&
        fuzz_png_apng_mutator_append_u8(
            data,
            pos,
            max_size,
            (unsigned char)(value & 0xffu));
}

static int
fuzz_png_apng_mutator_append_span(uint8_t *data,
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
fuzz_png_apng_mutator_choose_focus(uint32_t *state)
{
    unsigned int feature;
    unsigned int best;
    unsigned int hits;
    unsigned int best_hits;

    if (state == NULL) {
        return FUZZ_PNG_APNG_FEATURE_SIGNATURE;
    }

    best = (unsigned int)(fuzz_png_apng_mutator_next(state) %
                          FUZZ_PNG_APNG_FEATURE_COUNT);
    best_hits = g_png_apng_feature_hits[best];
    for (feature = 0u; feature < FUZZ_PNG_APNG_FEATURE_COUNT; ++feature) {
        hits = g_png_apng_feature_hits[feature];
        if (hits < best_hits ||
            (hits == best_hits &&
             (fuzz_png_apng_mutator_next(state) & 0x01u) != 0u)) {
            best = feature;
            best_hits = hits;
        }
    }

    return best;
}

static size_t
fuzz_png_apng_mutator_control_prefix(uint8_t const *data, size_t size)
{
    size_t prefix_size;

    if (data == NULL || size == 0u) {
        return 0u;
    }

    prefix_size = 19u;
    if (prefix_size > size) {
        prefix_size = size;
    }

    return prefix_size;
}

static int
fuzz_png_apng_mutator_append_chunk_ctl(uint8_t *data,
                                       size_t *pos,
                                       size_t max_size,
                                       uint32_t *state,
                                       int bad_crc)
{
    if (!fuzz_png_apng_mutator_append_u8(data,
                                         pos,
                                         max_size,
                                         bad_crc != 0 ? 1u : 0u)) {
        return 0;
    }

    return fuzz_png_apng_mutator_append_u32(
        data,
        pos,
        max_size,
        fuzz_png_apng_mutator_next(state));
}

static int
fuzz_png_apng_mutator_append_frame(uint8_t *data,
                                   size_t *pos,
                                   size_t max_size,
                                   uint32_t *state,
                                   unsigned char index)
{
    size_t j;

    if (!fuzz_png_apng_mutator_append_u8(data, pos, max_size, 31u) ||
        !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 31u) ||
        !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 1u) ||
        !fuzz_png_apng_mutator_append_u8(data, pos, max_size, index) ||
        !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 1u) ||
        !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 1u) ||
        !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 1u) ||
        !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 1u) ||
        !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 2u) ||
        !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 1u) ||
        !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 1u) ||
        !fuzz_png_apng_mutator_append_chunk_ctl(data,
                                                pos,
                                                max_size,
                                                state,
                                                index == 1u)) {
        return 0;
    }

    if (index == 0u) {
        if (!fuzz_png_apng_mutator_append_u8(data, pos, max_size, 0u) ||
            !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 0u) ||
            !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 0u) ||
            !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 0u)) {
            return 0;
        }
        return fuzz_png_apng_mutator_append_chunk_ctl(data,
                                                      pos,
                                                      max_size,
                                                      state,
                                                      1);
    }

    if (!fuzz_png_apng_mutator_append_u8(data, pos, max_size, 0u) ||
        !fuzz_png_apng_mutator_append_u8(data, pos, max_size, 8u)) {
        return 0;
    }
    for (j = 0u; j < 8u; ++j) {
        if (!fuzz_png_apng_mutator_append_u8(
                data,
                pos,
                max_size,
                (unsigned char)fuzz_png_apng_mutator_next(state))) {
            return 0;
        }
    }

    return fuzz_png_apng_mutator_append_chunk_ctl(data,
                                                  pos,
                                                  max_size,
                                                  state,
                                                  index == 2u);
}

static size_t
fuzz_png_apng_mutator_synthesize(uint8_t *data,
                                 size_t max_size,
                                 uint32_t *state,
                                 unsigned int focus)
{
    size_t pos;
    unsigned char frame_selector;
    int bad_ihdr_crc;
    int bad_actl_crc;

    if (data == NULL || state == NULL || max_size == 0u) {
        return 0u;
    }

    pos = 0u;
    frame_selector = focus == FUZZ_PNG_APNG_FEATURE_MULTI_FRAME ||
                     focus == FUZZ_PNG_APNG_FEATURE_FCTL ||
                     focus == FUZZ_PNG_APNG_FEATURE_FDAT ||
                     focus == FUZZ_PNG_APNG_FEATURE_DISPOSE ||
                     focus == FUZZ_PNG_APNG_FEATURE_BLEND ? 2u : 0u;
    bad_ihdr_crc = focus == FUZZ_PNG_APNG_FEATURE_BAD_CRC ||
                   (fuzz_png_apng_mutator_next(state) & 0x03u) != 0u;
    bad_actl_crc = focus == FUZZ_PNG_APNG_FEATURE_BAD_CRC ? 1 : 0;
    if (!fuzz_png_apng_mutator_append_u8(data, &pos, max_size, 63u) ||
        !fuzz_png_apng_mutator_append_u8(data, &pos, max_size, 63u) ||
        !fuzz_png_apng_mutator_append_chunk_ctl(data,
                                                &pos,
                                                max_size,
                                                state,
                                                bad_ihdr_crc) ||
        !fuzz_png_apng_mutator_append_u8(data,
                                         &pos,
                                         max_size,
                                         frame_selector) ||
        !fuzz_png_apng_mutator_append_u8(data, &pos, max_size, 1u) ||
        !fuzz_png_apng_mutator_append_u8(data, &pos, max_size, 1u) ||
        !fuzz_png_apng_mutator_append_chunk_ctl(data,
                                                &pos,
                                                max_size,
                                                state,
                                                bad_actl_crc) ||
        !fuzz_png_apng_mutator_append_u32(data,
                                          &pos,
                                          max_size,
                                          fuzz_png_apng_mutator_next(state))) {
        return pos;
    }

    if (!fuzz_png_apng_mutator_append_frame(data, &pos, max_size, state, 0u)) {
        return pos;
    }
    if (frame_selector != 0u) {
        if (!fuzz_png_apng_mutator_append_frame(data,
                                                &pos,
                                                max_size,
                                                state,
                                                1u) ||
            !fuzz_png_apng_mutator_append_frame(data,
                                                &pos,
                                                max_size,
                                                state,
                                                2u)) {
            return pos;
        }
    }

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

    state = (uint32_t)seed ^ (uint32_t)size ^ UINT32_C(0x27d4eb2d);
    focus = fuzz_png_apng_mutator_choose_focus(&state);
    if (size != 0u && (fuzz_png_apng_mutator_next(&state) & 0x07u) == 0u) {
        mutated_size = LLVMFuzzerMutate(data, size, max_size);
        if (mutated_size != 0u) {
            return mutated_size;
        }
    }

    return fuzz_png_apng_mutator_synthesize(data, max_size, &state, focus);
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
    size_t pos;
    size_t prefix_size;
    size_t suffix_offset;
    uint8_t const *prefix_data;
    uint8_t const *suffix_data;
    size_t prefix_data_size;
    size_t suffix_data_size;
    unsigned int focus;

    if (out == NULL || max_out_size == 0u) {
        return 0u;
    }

    state = (uint32_t)seed ^ (uint32_t)size1 ^
            ((uint32_t)size2 << 1) ^ UINT32_C(0xd3a2646c);
    focus = fuzz_png_apng_mutator_choose_focus(&state);
    if ((fuzz_png_apng_mutator_next(&state) & 0x01u) == 0u) {
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
    prefix_size = fuzz_png_apng_mutator_control_prefix(prefix_data,
                                                      prefix_data_size);
    if (prefix_size > 0u) {
        (void)fuzz_png_apng_mutator_append_span(out,
                                                &pos,
                                                max_out_size,
                                                prefix_data,
                                                prefix_size);
    }

    suffix_offset = fuzz_png_apng_mutator_control_prefix(suffix_data,
                                                        suffix_data_size);
    if (suffix_offset >= suffix_data_size) {
        suffix_offset = suffix_data_size / 2u;
    }
    if (suffix_offset < suffix_data_size) {
        (void)fuzz_png_apng_mutator_append_span(out,
                                                &pos,
                                                max_out_size,
                                                suffix_data + suffix_offset,
                                                suffix_data_size -
                                                    suffix_offset);
    }
    if (pos == 0u) {
        return fuzz_png_apng_mutator_synthesize(out,
                                                max_out_size,
                                                &state,
                                                focus);
    }
    if (focus == FUZZ_PNG_APNG_FEATURE_MULTI_FRAME && pos > 7u) {
        out[7] = 2u;
    }

    return pos;
}

static int
fuzz_png_apng_append_chunk(fuzz_byte_buffer_t *payload,
                           char const type[4],
                           unsigned char const *data,
                           size_t length,
                           int force_bad_crc,
                           uint32_t crc_salt)
{
    if (force_bad_crc != 0) {
        fuzz_png_apng_note_feature(FUZZ_PNG_APNG_FEATURE_BAD_CRC);
    }
    if (type[0] == 'I' && type[1] == 'H' &&
        type[2] == 'D' && type[3] == 'R') {
        fuzz_png_apng_note_feature(FUZZ_PNG_APNG_FEATURE_IHDR);
    } else if (type[0] == 'a' && type[1] == 'c' &&
               type[2] == 'T' && type[3] == 'L') {
        fuzz_png_apng_note_feature(FUZZ_PNG_APNG_FEATURE_ACTL);
    } else if (type[0] == 'f' && type[1] == 'c' &&
               type[2] == 'T' && type[3] == 'L') {
        fuzz_png_apng_note_feature(FUZZ_PNG_APNG_FEATURE_FCTL);
    } else if (type[0] == 'I' && type[1] == 'D' &&
               type[2] == 'A' && type[3] == 'T') {
        fuzz_png_apng_note_feature(FUZZ_PNG_APNG_FEATURE_IDAT);
    } else if (type[0] == 'f' && type[1] == 'd' &&
               type[2] == 'A' && type[3] == 'T') {
        fuzz_png_apng_note_feature(FUZZ_PNG_APNG_FEATURE_FDAT);
    }

    return fuzz_append_png_chunk(payload,
                                 type,
                                 data,
                                 length,
                                 force_bad_crc,
                                 crc_salt);
}

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
    if (!fuzz_byte_buffer_append(payload,
                                 png_signature,
                                 sizeof(png_signature))) {
        return 0;
    }
    fuzz_png_apng_note_feature(FUZZ_PNG_APNG_FEATURE_SIGNATURE);

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

    if (!fuzz_png_apng_append_chunk(
            payload,
            "IHDR",
            ihdr,
            sizeof(ihdr),
            (int)(fuzz_cursor_take_u8(&cursor, 0u) & 0x01u),
            (uint32_t)fuzz_cursor_take_u32be(&cursor, 1u))) {
        return 0;
    }

    frame_count = 1u + (uint32_t)(fuzz_cursor_take_u8(&cursor, 0u) % 3u);
    if (frame_count > 1u) {
        fuzz_png_apng_note_feature(FUZZ_PNG_APNG_FEATURE_MULTI_FRAME);
    }
    actl[0] = (unsigned char)((frame_count >> 24) & 0xffu);
    actl[1] = (unsigned char)((frame_count >> 16) & 0xffu);
    actl[2] = (unsigned char)((frame_count >> 8) & 0xffu);
    actl[3] = (unsigned char)(frame_count & 0xffu);
    actl[4] = 0u;
    actl[5] = 0u;
    actl[6] = 0u;
    actl[7] = fuzz_cursor_take_u8(&cursor, 0u);

    if ((fuzz_cursor_take_u8(&cursor, 1u) & 0x01u) != 0u) {
        if (!fuzz_png_apng_append_chunk(
                payload,
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
            fuzz_png_apng_note_feature(FUZZ_PNG_APNG_FEATURE_SEQUENCE_SKEW);
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
        if (fctl[24] != 0u) {
            fuzz_png_apng_note_feature(FUZZ_PNG_APNG_FEATURE_DISPOSE);
        }
        if (fctl[25] != 0u) {
            fuzz_png_apng_note_feature(FUZZ_PNG_APNG_FEATURE_BLEND);
        }

        if ((fuzz_cursor_take_u8(&cursor, 1u) & 0x01u) != 0u) {
            if (!fuzz_png_apng_append_chunk(
                    payload,
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
                fuzz_png_apng_note_feature(
                    FUZZ_PNG_APNG_FEATURE_FALLBACK_IDAT);
            }
            if (!fuzz_png_apng_append_chunk(
                    payload,
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

            if (!fuzz_png_apng_append_chunk(
                    payload,
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
        fuzz_png_apng_note_feature(FUZZ_PNG_APNG_FEATURE_FALLBACK_IDAT);
        if (!fuzz_png_apng_append_chunk(payload,
                                        "IDAT",
                                        fallback_idat,
                                        sizeof(fallback_idat),
                                        0,
                                        0u)) {
            return 0;
        }
    }

    if (!fuzz_png_apng_append_chunk(payload, "IEND", NULL, 0u, 0, 0u)) {
        return 0;
    }

    return 1;
}

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    fuzz_png_apng_register_semantic_stats();
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

    fuzz_png_apng_register_semantic_stats();
    fuzz_byte_buffer_init(&payload);
    if (!fuzz_build_png_apng_payload(data, size, &payload)) {
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
