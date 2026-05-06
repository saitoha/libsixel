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
#include <stdlib.h>

#include "fuzz-loader-builtin-struct-common.h"

extern size_t LLVMFuzzerMutate(uint8_t *data, size_t size, size_t max_size);

enum fuzz_sixel_feature {
    FUZZ_SIXEL_FEATURE_PIXEL = 0,
    FUZZ_SIXEL_FEATURE_PALETTE,
    FUZZ_SIXEL_FEATURE_REPEAT,
    FUZZ_SIXEL_FEATURE_RASTER,
    FUZZ_SIXEL_FEATURE_CONTROL,
    FUZZ_SIXEL_FEATURE_PARAM,
    FUZZ_SIXEL_FEATURE_RAW_TAIL,
    FUZZ_SIXEL_FEATURE_ESC_ST,
    FUZZ_SIXEL_FEATURE_C1_ST,
    FUZZ_SIXEL_FEATURE_HIGH_COLOR,
    FUZZ_SIXEL_FEATURE_LONG_REPEAT,
    FUZZ_SIXEL_FEATURE_LARGE_RASTER,
    FUZZ_SIXEL_FEATURE_COUNT
};

static unsigned int g_sixel_feature_mask = 0u;
static unsigned int g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_COUNT];
static int g_sixel_stats_registered = 0;

#define FUZZ_SIXEL_RECORD_FEATURE(_feature) \
    do { \
        g_sixel_feature_mask |= 1u << (_feature); \
        ++g_sixel_feature_hits[(_feature)]; \
    } while (0)

static unsigned int
fuzz_sixel_count_bits(unsigned int value)
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
fuzz_sixel_note_feature(unsigned int feature)
{
    if (feature >= FUZZ_SIXEL_FEATURE_COUNT) {
        return;
    }

    /*
     * Keep semantic milestones visible to coverage-guided fuzzers. Edge
     * coverage alone often treats distinct SIXEL states as the same parser
     * path, so each meaningful token family gets its own instrumented block.
     */
    switch (feature) {
    case FUZZ_SIXEL_FEATURE_PIXEL:
        FUZZ_SIXEL_RECORD_FEATURE(FUZZ_SIXEL_FEATURE_PIXEL);
        break;
    case FUZZ_SIXEL_FEATURE_PALETTE:
        FUZZ_SIXEL_RECORD_FEATURE(FUZZ_SIXEL_FEATURE_PALETTE);
        break;
    case FUZZ_SIXEL_FEATURE_REPEAT:
        FUZZ_SIXEL_RECORD_FEATURE(FUZZ_SIXEL_FEATURE_REPEAT);
        break;
    case FUZZ_SIXEL_FEATURE_RASTER:
        FUZZ_SIXEL_RECORD_FEATURE(FUZZ_SIXEL_FEATURE_RASTER);
        break;
    case FUZZ_SIXEL_FEATURE_CONTROL:
        FUZZ_SIXEL_RECORD_FEATURE(FUZZ_SIXEL_FEATURE_CONTROL);
        break;
    case FUZZ_SIXEL_FEATURE_PARAM:
        FUZZ_SIXEL_RECORD_FEATURE(FUZZ_SIXEL_FEATURE_PARAM);
        break;
    case FUZZ_SIXEL_FEATURE_RAW_TAIL:
        FUZZ_SIXEL_RECORD_FEATURE(FUZZ_SIXEL_FEATURE_RAW_TAIL);
        break;
    case FUZZ_SIXEL_FEATURE_ESC_ST:
        FUZZ_SIXEL_RECORD_FEATURE(FUZZ_SIXEL_FEATURE_ESC_ST);
        break;
    case FUZZ_SIXEL_FEATURE_C1_ST:
        FUZZ_SIXEL_RECORD_FEATURE(FUZZ_SIXEL_FEATURE_C1_ST);
        break;
    case FUZZ_SIXEL_FEATURE_HIGH_COLOR:
        FUZZ_SIXEL_RECORD_FEATURE(FUZZ_SIXEL_FEATURE_HIGH_COLOR);
        break;
    case FUZZ_SIXEL_FEATURE_LONG_REPEAT:
        FUZZ_SIXEL_RECORD_FEATURE(FUZZ_SIXEL_FEATURE_LONG_REPEAT);
        break;
    case FUZZ_SIXEL_FEATURE_LARGE_RASTER:
        FUZZ_SIXEL_RECORD_FEATURE(FUZZ_SIXEL_FEATURE_LARGE_RASTER);
        break;
    default:
        break;
    }
}

static void
fuzz_sixel_print_semantic_stats(void)
{
    fprintf(stderr,
            "sixel-stream-semantic-features: mask=0x%08x count=%u "
            "pixel=%u palette=%u repeat=%u raster=%u control=%u "
            "param=%u raw-tail=%u esc-st=%u c1-st=%u high-color=%u "
            "long-repeat=%u large-raster=%u\n",
            g_sixel_feature_mask,
            fuzz_sixel_count_bits(g_sixel_feature_mask),
            g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_PIXEL],
            g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_PALETTE],
            g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_REPEAT],
            g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_RASTER],
            g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_CONTROL],
            g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_PARAM],
            g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_RAW_TAIL],
            g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_ESC_ST],
            g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_C1_ST],
            g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_HIGH_COLOR],
            g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_LONG_REPEAT],
            g_sixel_feature_hits[FUZZ_SIXEL_FEATURE_LARGE_RASTER]);
}

static void
fuzz_sixel_register_semantic_stats(void)
{
    if (g_sixel_stats_registered) {
        return;
    }

    (void)atexit(fuzz_sixel_print_semantic_stats);
    g_sixel_stats_registered = 1;
}

static uint32_t
fuzz_sixel_mutator_next(uint32_t *state)
{
    uint32_t value;

    value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    if (value == 0u) {
        value = UINT32_C(0x9e3779b9);
    }
    *state = value;

    return value;
}

static int
fuzz_sixel_mutator_append_u8(uint8_t *data,
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

static size_t
fuzz_sixel_mutator_param_count(unsigned char kind)
{
    switch (kind % 6u) {
    case 0u:
    case 4u:
    case 5u:
        return 1u;
    case 2u:
        return 2u;
    case 1u:
    case 3u:
        return 4u;
    default:
        break;
    }

    return 0u;
}

static int
fuzz_sixel_mutator_emit_token(uint8_t *data,
                              size_t *pos,
                              size_t max_size,
                              unsigned char kind,
                              uint32_t *state)
{
    unsigned int value;

    if (!fuzz_sixel_mutator_append_u8(data, pos, max_size, kind)) {
        return 0;
    }

    switch (kind % 6u) {
    case 0u:
        value = 0x3fu + (fuzz_sixel_mutator_next(state) % 64u);
        return fuzz_sixel_mutator_append_u8(data,
                                            pos,
                                            max_size,
                                            (unsigned char)value);
    case 1u:
        if (!fuzz_sixel_mutator_append_u8(data,
                                          pos,
                                          max_size,
                                          (unsigned char)
                                              (fuzz_sixel_mutator_next(state)
                                               % 32u))) {
            return 0;
        }
        if (!fuzz_sixel_mutator_append_u8(data,
                                          pos,
                                          max_size,
                                          (unsigned char)
                                              (fuzz_sixel_mutator_next(state)
                                               % 101u))) {
            return 0;
        }
        if (!fuzz_sixel_mutator_append_u8(data,
                                          pos,
                                          max_size,
                                          (unsigned char)
                                              (fuzz_sixel_mutator_next(state)
                                               % 101u))) {
            return 0;
        }
        return fuzz_sixel_mutator_append_u8(data,
                                            pos,
                                            max_size,
                                            (unsigned char)
                                                (fuzz_sixel_mutator_next(state)
                                                 % 101u));
    case 2u:
        if (!fuzz_sixel_mutator_append_u8(data,
                                          pos,
                                          max_size,
                                          (unsigned char)
                                              (1u +
                                               (fuzz_sixel_mutator_next(state)
                                                % 128u)))) {
            return 0;
        }
        value = 0x3fu + (fuzz_sixel_mutator_next(state) % 64u);
        return fuzz_sixel_mutator_append_u8(data,
                                            pos,
                                            max_size,
                                            (unsigned char)value);
    case 3u:
        if (!fuzz_sixel_mutator_append_u8(data,
                                          pos,
                                          max_size,
                                          (unsigned char)
                                              (1u +
                                               (fuzz_sixel_mutator_next(state)
                                                % 4u)))) {
            return 0;
        }
        if (!fuzz_sixel_mutator_append_u8(data,
                                          pos,
                                          max_size,
                                          (unsigned char)
                                              (1u +
                                               (fuzz_sixel_mutator_next(state)
                                                % 4u)))) {
            return 0;
        }
        if (!fuzz_sixel_mutator_append_u8(data,
                                          pos,
                                          max_size,
                                          (unsigned char)
                                              (1u +
                                               (fuzz_sixel_mutator_next(state)
                                                % 160u)))) {
            return 0;
        }
        return fuzz_sixel_mutator_append_u8(data,
                                            pos,
                                            max_size,
                                            (unsigned char)
                                                (1u +
                                                 (fuzz_sixel_mutator_next(state)
                                                  % 160u)));
    case 4u:
        return fuzz_sixel_mutator_append_u8(data,
                                            pos,
                                            max_size,
                                            (unsigned char)
                                                (fuzz_sixel_mutator_next(state)
                                                 & 1u));
    default:
        return fuzz_sixel_mutator_append_u8(data,
                                            pos,
                                            max_size,
                                            (unsigned char)
                                                (fuzz_sixel_mutator_next(state)
                                                 & 1u));
    }
}

static void
fuzz_sixel_mutator_repair(uint8_t *data, size_t size, uint32_t *state)
{
    size_t pos;
    size_t skip;
    unsigned char kind;

    if (data == NULL || size == 0u) {
        return;
    }

    data[0] = (uint8_t)(fuzz_sixel_mutator_next(state) % 96u);
    pos = 1u;
    kind = 0u;
    while (kind < 6u && pos < size) {
        data[pos] = kind;
        skip = 1u + fuzz_sixel_mutator_param_count(kind);
        if (skip > size - pos) {
            break;
        }
        pos += skip;
        ++kind;
    }
}

size_t
LLVMFuzzerCustomMutator(uint8_t *data,
                        size_t size,
                        size_t max_size,
                        unsigned int seed)
{
    uint32_t state;
    size_t pos;
    size_t token_count;
    size_t tail_count;
    size_t mutated_size;
    size_t i;
    unsigned char kind;

    if (data == NULL || max_size == 0u) {
        return 0u;
    }

    state = ((uint32_t)seed ^ (uint32_t)size ^ UINT32_C(0xa5a5c3c3));

    /*
     * Preserve libFuzzer's general-purpose mutations some of the time, then
     * repair the early control stream so the parser still sees each token
     * family quickly.
     */
    if (size > 0u && (seed & 3u) == 0u) {
        mutated_size = LLVMFuzzerMutate(data, size, max_size);
        if (mutated_size == 0u) {
            data[0] = 0u;
            mutated_size = 1u;
        }
        fuzz_sixel_mutator_repair(data, mutated_size, &state);
        return mutated_size;
    }

    token_count = 8u + (size_t)(fuzz_sixel_mutator_next(&state) % 96u);
    data[0] = (uint8_t)(token_count - 8u);
    pos = 1u;

    for (i = 0u; i < token_count && pos < max_size; ++i) {
        if (i < 6u) {
            kind = (unsigned char)i;
        } else {
            kind = (unsigned char)(fuzz_sixel_mutator_next(&state) % 6u);
        }
        if (!fuzz_sixel_mutator_emit_token(data,
                                           &pos,
                                           max_size,
                                           kind,
                                           &state)) {
            break;
        }
    }

    tail_count = (size_t)(fuzz_sixel_mutator_next(&state) % 16u);
    for (i = 0u; i < tail_count && pos < max_size; ++i) {
        kind = (unsigned char)(0x3fu +
                               (fuzz_sixel_mutator_next(&state) % 64u));
        if (!fuzz_sixel_mutator_append_u8(data, &pos, max_size, kind)) {
            break;
        }
    }

    return pos;
}

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
    size_t tail_size;
    size_t i;
    unsigned char kind;
    unsigned char ch;
    unsigned int color_index;
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int repeat_count;
    unsigned int raster_pan;
    unsigned int raster_pad;
    unsigned int raster_width;
    unsigned int raster_height;

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
        kind = (unsigned char)(fuzz_cursor_take_u8(&cursor, 0u) % 6u);
        switch (kind) {
        case 0u:
            ch = (unsigned char)(0x3fu +
                                 (fuzz_cursor_take_u8(&cursor, 0u) % 64u));
            fuzz_sixel_note_feature(FUZZ_SIXEL_FEATURE_PIXEL);
            if (!fuzz_byte_buffer_append_u8(payload, ch)) {
                return 0;
            }
            break;
        case 1u:
            color_index = (unsigned int)
                (fuzz_cursor_take_u8(&cursor, 0u) % 32u);
            red = (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 101u);
            green = (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 101u);
            blue = (unsigned int)(fuzz_cursor_take_u8(&cursor, 0u) % 101u);
            fuzz_sixel_note_feature(FUZZ_SIXEL_FEATURE_PALETTE);
            if (color_index >= 16u || red >= 90u ||
                green >= 90u || blue >= 90u) {
                fuzz_sixel_note_feature(FUZZ_SIXEL_FEATURE_HIGH_COLOR);
            }
            if (!fuzz_append_printf(payload,
                                    "#%u;2;%u;%u;%u",
                                    color_index,
                                    red,
                                    green,
                                    blue)) {
                return 0;
            }
            break;
        case 2u:
            repeat_count = (unsigned int)(1u +
                                          (fuzz_cursor_take_u8(&cursor, 0u) %
                                           128u));
            ch = (unsigned char)(0x3fu +
                                 (fuzz_cursor_take_u8(&cursor, 0u) % 64u));
            fuzz_sixel_note_feature(FUZZ_SIXEL_FEATURE_REPEAT);
            if (repeat_count >= 96u) {
                fuzz_sixel_note_feature(FUZZ_SIXEL_FEATURE_LONG_REPEAT);
            }
            if (!fuzz_append_printf(payload,
                                    "!%u%c",
                                    repeat_count,
                                    (unsigned int)ch)) {
                return 0;
            }
            break;
        case 3u:
            raster_pan = (unsigned int)(1u +
                                        (fuzz_cursor_take_u8(&cursor, 0u) %
                                         4u));
            raster_pad = (unsigned int)(1u +
                                        (fuzz_cursor_take_u8(&cursor, 0u) %
                                         4u));
            raster_width = (unsigned int)(1u +
                                          (fuzz_cursor_take_u8(&cursor, 0u) %
                                           160u));
            raster_height = (unsigned int)(1u +
                                           (fuzz_cursor_take_u8(&cursor, 0u) %
                                            160u));
            fuzz_sixel_note_feature(FUZZ_SIXEL_FEATURE_RASTER);
            if (raster_width >= 128u || raster_height >= 128u) {
                fuzz_sixel_note_feature(FUZZ_SIXEL_FEATURE_LARGE_RASTER);
            }
            if (!fuzz_append_printf(payload,
                                    "\"%u;%u;%u;%u",
                                    raster_pan,
                                    raster_pad,
                                    raster_width,
                                    raster_height)) {
                return 0;
            }
            break;
        case 4u:
            fuzz_sixel_note_feature(FUZZ_SIXEL_FEATURE_CONTROL);
            if (!fuzz_byte_buffer_append_u8(payload,
                                            (fuzz_cursor_take_u8(&cursor,
                                                                 0u) &
                                             0x01u) != 0u
                                                ? (unsigned char)'$'
                                                : (unsigned char)'-')) {
                return 0;
            }
            break;
        default:
            fuzz_sixel_note_feature(FUZZ_SIXEL_FEATURE_PARAM);
            if (!fuzz_byte_buffer_append_u8(payload,
                                            (unsigned char)
                                                (fuzz_cursor_take_u8(&cursor,
                                                                     0u) % 2u
                                                 ? ';'
                                                 : '?'))) {
                return 0;
            }
            break;
        }
    }

    if (fuzz_cursor_remaining(&cursor) > 0u) {
        tail_size = fuzz_cursor_remaining(&cursor);
        if (tail_size > 128u) {
            tail_size = 128u;
        }
        if (!fuzz_byte_buffer_append(payload,
                                     cursor.data + cursor.pos,
                                     tail_size)) {
            return 0;
        }
        fuzz_sixel_note_feature(FUZZ_SIXEL_FEATURE_RAW_TAIL);
    }

    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) == 0u) {
        fuzz_sixel_note_feature(FUZZ_SIXEL_FEATURE_ESC_ST);
        return fuzz_byte_buffer_append(payload,
                                       (unsigned char const *)"\x1b\\",
                                       2u);
    }

    fuzz_sixel_note_feature(FUZZ_SIXEL_FEATURE_C1_ST);
    return fuzz_byte_buffer_append_u8(payload, 0x9cu);
}

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    (void)fuzz_loader_builtin_runtime_bootstrap();
    fuzz_sixel_register_semantic_stats();
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
    fuzz_sixel_register_semantic_stats();

    fuzz_byte_buffer_init(&payload);
    if (!fuzz_build_sixel_stream_payload(data, size, &payload)) {
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
