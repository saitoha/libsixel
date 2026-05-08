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

enum fuzz_gif_block_feature {
    FUZZ_GIF_BLOCK_FEATURE_HEADER = 0,
    FUZZ_GIF_BLOCK_FEATURE_GCE,
    FUZZ_GIF_BLOCK_FEATURE_TRANSPARENT_GCE,
    FUZZ_GIF_BLOCK_FEATURE_COMMENT,
    FUZZ_GIF_BLOCK_FEATURE_APPLICATION,
    FUZZ_GIF_BLOCK_FEATURE_PLAIN_TEXT,
    FUZZ_GIF_BLOCK_FEATURE_IMAGE,
    FUZZ_GIF_BLOCK_FEATURE_GLOBAL_PALETTE,
    FUZZ_GIF_BLOCK_FEATURE_LOCAL_PALETTE,
    FUZZ_GIF_BLOCK_FEATURE_INTERLACE,
    FUZZ_GIF_BLOCK_FEATURE_SUBBLOCK,
    FUZZ_GIF_BLOCK_FEATURE_LOOP,
    FUZZ_GIF_BLOCK_FEATURE_TRAILER,
    FUZZ_GIF_BLOCK_FEATURE_COUNT
};

static unsigned int g_gif_block_feature_mask = 0u;
static unsigned int g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_COUNT];
static int g_gif_block_stats_registered = 0;

#define FUZZ_GIF_BLOCK_RECORD_FEATURE(_feature) \
    do { \
        g_gif_block_feature_mask |= 1u << (_feature); \
        ++g_gif_block_feature_hits[(_feature)]; \
    } while (0)

static unsigned int
fuzz_gif_block_count_bits(unsigned int value)
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
fuzz_gif_block_note_feature(unsigned int feature)
{
    if (feature >= FUZZ_GIF_BLOCK_FEATURE_COUNT) {
        return;
    }

    /*
     * Extension-heavy GIFs often share loader edges. These branches make the
     * block grammar visible as fuzzing milestones.
     */
    switch (feature) {
    case FUZZ_GIF_BLOCK_FEATURE_HEADER:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_HEADER);
        break;
    case FUZZ_GIF_BLOCK_FEATURE_GCE:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_GCE);
        break;
    case FUZZ_GIF_BLOCK_FEATURE_TRANSPARENT_GCE:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_TRANSPARENT_GCE);
        break;
    case FUZZ_GIF_BLOCK_FEATURE_COMMENT:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_COMMENT);
        break;
    case FUZZ_GIF_BLOCK_FEATURE_APPLICATION:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_APPLICATION);
        break;
    case FUZZ_GIF_BLOCK_FEATURE_PLAIN_TEXT:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_PLAIN_TEXT);
        break;
    case FUZZ_GIF_BLOCK_FEATURE_IMAGE:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_IMAGE);
        break;
    case FUZZ_GIF_BLOCK_FEATURE_GLOBAL_PALETTE:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_GLOBAL_PALETTE);
        break;
    case FUZZ_GIF_BLOCK_FEATURE_LOCAL_PALETTE:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_LOCAL_PALETTE);
        break;
    case FUZZ_GIF_BLOCK_FEATURE_INTERLACE:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_INTERLACE);
        break;
    case FUZZ_GIF_BLOCK_FEATURE_SUBBLOCK:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_SUBBLOCK);
        break;
    case FUZZ_GIF_BLOCK_FEATURE_LOOP:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_LOOP);
        break;
    case FUZZ_GIF_BLOCK_FEATURE_TRAILER:
        FUZZ_GIF_BLOCK_RECORD_FEATURE(FUZZ_GIF_BLOCK_FEATURE_TRAILER);
        break;
    default:
        break;
    }
}

static void
fuzz_gif_block_print_semantic_stats(void)
{
    fprintf(stderr,
            "gif-block-semantic-features: mask=0x%08x count=%u "
            "header=%u gce=%u transparent-gce=%u comment=%u "
            "application=%u plain-text=%u image=%u global-palette=%u "
            "local-palette=%u interlace=%u subblock=%u loop=%u "
            "trailer=%u\n",
            g_gif_block_feature_mask,
            fuzz_gif_block_count_bits(g_gif_block_feature_mask),
            g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_HEADER],
            g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_GCE],
            g_gif_block_feature_hits[
                FUZZ_GIF_BLOCK_FEATURE_TRANSPARENT_GCE],
            g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_COMMENT],
            g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_APPLICATION],
            g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_PLAIN_TEXT],
            g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_IMAGE],
            g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_GLOBAL_PALETTE],
            g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_LOCAL_PALETTE],
            g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_INTERLACE],
            g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_SUBBLOCK],
            g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_LOOP],
            g_gif_block_feature_hits[FUZZ_GIF_BLOCK_FEATURE_TRAILER]);
}

static void
fuzz_gif_block_register_semantic_stats(void)
{
    if (g_gif_block_stats_registered) {
        return;
    }

    (void)atexit(fuzz_gif_block_print_semantic_stats);
    g_gif_block_stats_registered = 1;
}

static uint32_t
fuzz_gif_block_mutator_next(uint32_t *state)
{
    uint32_t value;

    value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    if (value == 0u) {
        value = UINT32_C(0x632be59b);
    }
    *state = value;

    return value;
}

static int
fuzz_gif_block_mutator_append_u8(uint8_t *data,
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
fuzz_gif_block_mutator_append_u16(uint8_t *data,
                                  size_t *pos,
                                  size_t max_size,
                                  uint16_t value)
{
    return fuzz_gif_block_mutator_append_u8(
        data,
        pos,
        max_size,
        (unsigned char)((value >> 8) & 0xffu)) &&
        fuzz_gif_block_mutator_append_u8(
            data,
            pos,
            max_size,
            (unsigned char)(value & 0xffu));
}

static int
fuzz_gif_block_mutator_append_span(uint8_t *data,
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
fuzz_gif_block_mutator_choose_focus(uint32_t *state)
{
    unsigned int feature;
    unsigned int best;
    unsigned int hits;
    unsigned int best_hits;

    if (state == NULL) {
        return FUZZ_GIF_BLOCK_FEATURE_HEADER;
    }

    best = (unsigned int)(fuzz_gif_block_mutator_next(state) %
                          FUZZ_GIF_BLOCK_FEATURE_COUNT);
    best_hits = g_gif_block_feature_hits[best];
    for (feature = 0u; feature < FUZZ_GIF_BLOCK_FEATURE_COUNT; ++feature) {
        hits = g_gif_block_feature_hits[feature];
        if (hits < best_hits ||
            (hits == best_hits &&
             (fuzz_gif_block_mutator_next(state) & 0x01u) != 0u)) {
            best = feature;
            best_hits = hits;
        }
    }

    return best;
}

static int
fuzz_gif_block_mutator_append_subblock(uint8_t *data,
                                       size_t *pos,
                                       size_t max_size,
                                       uint32_t *state,
                                       unsigned char payload_size)
{
    unsigned char i;

    if (payload_size == 0u) {
        payload_size = 1u;
    }

    if (!fuzz_gif_block_mutator_append_u8(data, pos, max_size, 0u) ||
        !fuzz_gif_block_mutator_append_u8(data,
                                          pos,
                                          max_size,
                                          payload_size - 1u)) {
        return 0;
    }

    for (i = 0u; i < payload_size; ++i) {
        if (!fuzz_gif_block_mutator_append_u8(
                data,
                pos,
                max_size,
                (unsigned char)fuzz_gif_block_mutator_next(state))) {
            return 0;
        }
    }

    return fuzz_gif_block_mutator_append_u8(data, pos, max_size, 0u);
}

static size_t
fuzz_gif_block_mutator_synthesize(uint8_t *data,
                                  size_t max_size,
                                  uint32_t *state,
                                  unsigned int focus)
{
    size_t pos;
    unsigned char i;
    unsigned char gce_packed;
    unsigned char image_packed;

    if (data == NULL || state == NULL || max_size == 0u) {
        return 0u;
    }

    pos = 0u;
    gce_packed = focus == FUZZ_GIF_BLOCK_FEATURE_GCE ? 0u : 1u;
    image_packed = focus == FUZZ_GIF_BLOCK_FEATURE_IMAGE ? 0u : 0xc0u;
    if (!fuzz_gif_block_mutator_append_u8(data, &pos, max_size, 95u) ||
        !fuzz_gif_block_mutator_append_u8(data, &pos, max_size, 95u)) {
        return pos;
    }
    for (i = 0u; i < 6u; ++i) {
        if (!fuzz_gif_block_mutator_append_u8(
                data,
                &pos,
                max_size,
                (unsigned char)fuzz_gif_block_mutator_next(state))) {
            return pos;
        }
    }

    if (!fuzz_gif_block_mutator_append_u8(data, &pos, max_size, 4u) ||
        !fuzz_gif_block_mutator_append_u8(data, &pos, max_size, 0u) ||
        !fuzz_gif_block_mutator_append_u8(data,
                                          &pos,
                                          max_size,
                                          gce_packed) ||
        !fuzz_gif_block_mutator_append_u16(data, &pos, max_size, 1u) ||
        !fuzz_gif_block_mutator_append_u8(data, &pos, max_size, 1u) ||
        !fuzz_gif_block_mutator_append_u8(data, &pos, max_size, 1u)) {
        return pos;
    }

    if (!fuzz_gif_block_mutator_append_u8(data, &pos, max_size, 1u) ||
        !fuzz_gif_block_mutator_append_subblock(data,
                                                &pos,
                                                max_size,
                                                state,
                                                4u) ||
        !fuzz_gif_block_mutator_append_u8(data, &pos, max_size, 2u) ||
        !fuzz_gif_block_mutator_append_subblock(data,
                                                &pos,
                                                max_size,
                                                state,
                                                3u) ||
        !fuzz_gif_block_mutator_append_u8(data, &pos, max_size, 3u) ||
        !fuzz_gif_block_mutator_append_subblock(data,
                                                &pos,
                                                max_size,
                                                state,
                                                3u)) {
        return pos;
    }

    if (!fuzz_gif_block_mutator_append_u8(data, &pos, max_size, 0u) ||
        !fuzz_gif_block_mutator_append_u8(data, &pos, max_size, 0u) ||
        !fuzz_gif_block_mutator_append_u8(data,
                                          &pos,
                                          max_size,
                                          image_packed)) {
        return pos;
    }
    for (i = 0u; i < 6u; ++i) {
        if (!fuzz_gif_block_mutator_append_u8(
                data,
                &pos,
                max_size,
                (unsigned char)fuzz_gif_block_mutator_next(state))) {
            return pos;
        }
    }
    if (!fuzz_gif_block_mutator_append_u8(data, &pos, max_size, 4u) ||
        !fuzz_gif_block_mutator_append_subblock(data,
                                                &pos,
                                                max_size,
                                                state,
                                                8u)) {
        return pos;
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

    state = (uint32_t)seed ^ (uint32_t)size ^ UINT32_C(0x7feb352d);
    focus = fuzz_gif_block_mutator_choose_focus(&state);
    if (size != 0u && (fuzz_gif_block_mutator_next(&state) & 0x07u) == 0u) {
        mutated_size = LLVMFuzzerMutate(data, size, max_size);
        if (mutated_size != 0u) {
            return mutated_size;
        }
    }

    return fuzz_gif_block_mutator_synthesize(data, max_size, &state, focus);
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
            ((uint32_t)size2 << 1) ^ UINT32_C(0x846ca68b);
    focus = fuzz_gif_block_mutator_choose_focus(&state);
    if ((fuzz_gif_block_mutator_next(&state) & 0x01u) == 0u) {
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
    prefix_size = prefix_data_size < 11u ? prefix_data_size : 11u;
    if (prefix_data != NULL && prefix_size > 0u) {
        (void)fuzz_gif_block_mutator_append_span(out,
                                                 &pos,
                                                 max_out_size,
                                                 prefix_data,
                                                 prefix_size);
    }

    suffix_offset = suffix_data_size < 11u ? suffix_data_size : 11u;
    if (suffix_data != NULL && suffix_offset < suffix_data_size) {
        (void)fuzz_gif_block_mutator_append_span(
            out,
            &pos,
            max_out_size,
            suffix_data + suffix_offset,
            suffix_data_size - suffix_offset);
    }
    if (pos == 0u) {
        return fuzz_gif_block_mutator_synthesize(out,
                                                 max_out_size,
                                                 &state,
                                                 focus);
    }

    return pos;
}

static int
fuzz_append_gif_subblocks(fuzz_byte_buffer_t *buffer,
                          fuzz_cursor_t *cursor,
                          size_t max_blocks)
{
    size_t i;

    if (buffer == NULL || cursor == NULL) {
        return 0;
    }

    for (i = 0u; i < max_blocks; ++i) {
        size_t chunk_size;

        if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) == 0u && i > 0u) {
            break;
        }

        chunk_size = (size_t)(1u + (fuzz_cursor_take_u8(cursor, 0u) % 32u));
        if (chunk_size > fuzz_cursor_remaining(cursor)) {
            chunk_size = fuzz_cursor_remaining(cursor);
        }

        if (chunk_size == 0u) {
            break;
        }

        fuzz_gif_block_note_feature(FUZZ_GIF_BLOCK_FEATURE_SUBBLOCK);
        if (!fuzz_byte_buffer_append_u8(buffer, (unsigned char)chunk_size) ||
            !fuzz_byte_buffer_append(buffer,
                                     cursor->data + cursor->pos,
                                     chunk_size)) {
            return 0;
        }
        cursor->pos += chunk_size;
    }

    return fuzz_byte_buffer_append_u8(buffer, 0u);
}

static int
fuzz_build_gif_block_payload(uint8_t const *data,
                             size_t size,
                             fuzz_byte_buffer_t *payload)
{
    fuzz_cursor_t cursor;
    uint16_t width;
    uint16_t height;
    uint16_t image_left;
    uint16_t image_top;
    size_t extension_count;
    size_t i;
    unsigned char gce_packed;
    unsigned char image_packed;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    width = (uint16_t)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 96u));
    height = (uint16_t)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 96u));

    if (!fuzz_byte_buffer_append(payload,
                                 (unsigned char const *)"GIF89a",
                                 6u) ||
        !fuzz_byte_buffer_append_u16le(payload, width) ||
        !fuzz_byte_buffer_append_u16le(payload, height) ||
        !fuzz_byte_buffer_append_u8(payload, 0x80u) ||
        !fuzz_byte_buffer_append_u8(payload, 0u) ||
        !fuzz_byte_buffer_append_u8(payload, 0u) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    fuzz_cursor_take_u8(&cursor, 0u)) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    fuzz_cursor_take_u8(&cursor, 0u)) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    fuzz_cursor_take_u8(&cursor, 0u)) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    fuzz_cursor_take_u8(&cursor, 0u)) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    fuzz_cursor_take_u8(&cursor, 0u)) ||
        !fuzz_byte_buffer_append_u8(payload,
                                    fuzz_cursor_take_u8(&cursor, 0u))) {
        return 0;
    }
    fuzz_gif_block_note_feature(FUZZ_GIF_BLOCK_FEATURE_HEADER);
    fuzz_gif_block_note_feature(FUZZ_GIF_BLOCK_FEATURE_GLOBAL_PALETTE);

    extension_count = (size_t)(fuzz_cursor_take_u8(&cursor, 0u) % 5u);
    for (i = 0u; i < extension_count; ++i) {
        unsigned char ext_kind;

        ext_kind = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 4u);
        if (!fuzz_byte_buffer_append_u8(payload, 0x21u)) {
            return 0;
        }

        switch (ext_kind) {
        case 0u: {
            gce_packed = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) &
                                         0x1fu);
            fuzz_gif_block_note_feature(FUZZ_GIF_BLOCK_FEATURE_GCE);
            if ((gce_packed & 0x01u) != 0u) {
                fuzz_gif_block_note_feature(
                    FUZZ_GIF_BLOCK_FEATURE_TRANSPARENT_GCE);
            }
            if (!fuzz_byte_buffer_append_u8(payload, 0xf9u) ||
                !fuzz_byte_buffer_append_u8(payload, 4u) ||
                !fuzz_byte_buffer_append_u8(payload, gce_packed) ||
                !fuzz_byte_buffer_append_u16le(
                    payload,
                    (uint16_t)fuzz_cursor_take_u16be(&cursor, 0u)) ||
                !fuzz_byte_buffer_append_u8(payload,
                                            fuzz_cursor_take_u8(&cursor, 0u)) ||
                !fuzz_byte_buffer_append_u8(payload, 0u)) {
                return 0;
            }
            break;
        }
        case 1u:
            fuzz_gif_block_note_feature(FUZZ_GIF_BLOCK_FEATURE_COMMENT);
            if (!fuzz_byte_buffer_append_u8(payload, 0xfeu) ||
                !fuzz_append_gif_subblocks(payload, &cursor, 4u)) {
                return 0;
            }
            break;
        case 2u:
            fuzz_gif_block_note_feature(FUZZ_GIF_BLOCK_FEATURE_APPLICATION);
            fuzz_gif_block_note_feature(FUZZ_GIF_BLOCK_FEATURE_LOOP);
            if (!fuzz_byte_buffer_append_u8(payload, 0xffu) ||
                !fuzz_byte_buffer_append_u8(payload, 11u) ||
                !fuzz_byte_buffer_append(payload,
                                         (unsigned char const *)"NETSCAPE2.0",
                                         11u) ||
                !fuzz_append_gif_subblocks(payload, &cursor, 3u)) {
                return 0;
            }
            break;
        default:
            fuzz_gif_block_note_feature(FUZZ_GIF_BLOCK_FEATURE_PLAIN_TEXT);
            if (!fuzz_byte_buffer_append_u8(payload, 0x01u) ||
                !fuzz_byte_buffer_append_u8(payload, 12u) ||
                !fuzz_byte_buffer_append_zeros(payload, 12u) ||
                !fuzz_append_gif_subblocks(payload, &cursor, 3u)) {
                return 0;
            }
            break;
        }
    }

    image_left = (uint16_t)(fuzz_cursor_take_u8(&cursor, 0u) % 4u);
    image_top = (uint16_t)(fuzz_cursor_take_u8(&cursor, 0u) % 4u);
    image_packed = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) & 0xc0u);
    fuzz_gif_block_note_feature(FUZZ_GIF_BLOCK_FEATURE_IMAGE);
    if ((image_packed & 0x80u) != 0u) {
        fuzz_gif_block_note_feature(FUZZ_GIF_BLOCK_FEATURE_LOCAL_PALETTE);
    }
    if ((image_packed & 0x40u) != 0u) {
        fuzz_gif_block_note_feature(FUZZ_GIF_BLOCK_FEATURE_INTERLACE);
    }

    if (!fuzz_byte_buffer_append_u8(payload, 0x2cu) ||
        !fuzz_byte_buffer_append_u16le(payload, image_left) ||
        !fuzz_byte_buffer_append_u16le(payload, image_top) ||
        !fuzz_byte_buffer_append_u16le(payload, width) ||
        !fuzz_byte_buffer_append_u16le(payload, height) ||
        !fuzz_byte_buffer_append_u8(payload, image_packed)) {
        return 0;
    }
    if ((image_packed & 0x80u) != 0u &&
        (!fuzz_byte_buffer_append_u8(payload,
                                     fuzz_cursor_take_u8(&cursor, 0u)) ||
         !fuzz_byte_buffer_append_u8(payload,
                                     fuzz_cursor_take_u8(&cursor, 0u)) ||
         !fuzz_byte_buffer_append_u8(payload,
                                     fuzz_cursor_take_u8(&cursor, 0u)) ||
         !fuzz_byte_buffer_append_u8(payload,
                                     fuzz_cursor_take_u8(&cursor, 0u)) ||
         !fuzz_byte_buffer_append_u8(payload,
                                     fuzz_cursor_take_u8(&cursor, 0u)) ||
         !fuzz_byte_buffer_append_u8(payload,
                                     fuzz_cursor_take_u8(&cursor, 0u)))) {
        return 0;
    }

    if (!fuzz_byte_buffer_append_u8(
            payload,
            (unsigned char)(2u + (fuzz_cursor_take_u8(&cursor, 0u) % 6u))) ||
        !fuzz_append_gif_subblocks(payload, &cursor, 6u)) {
        return 0;
    }

    fuzz_gif_block_note_feature(FUZZ_GIF_BLOCK_FEATURE_TRAILER);
    if (!fuzz_byte_buffer_append_u8(payload, 0x3bu)) {
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
    fuzz_gif_block_register_semantic_stats();
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
    fuzz_gif_block_register_semantic_stats();

    fuzz_byte_buffer_init(&payload);
    if (!fuzz_build_gif_block_payload(data, size, &payload)) {
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
