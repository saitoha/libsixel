/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif /* HAVE_LIMITS_H */

#include <sixel.h>

#include "decoder.h"
#include "decoder-parallel.h"
#if SIXEL_ENABLE_THREADS
# include "decoder-columns.h"
# include "timeline-logger.h"
#endif
#include "compat_stub.h"
#include "options.h"
#include "pthread-once.h"
#include "threading.h"

/*
 * Parallel decoding starts only after the serial parser has established
 * raster bounds and an initial palette state.  Worker byte spans are allowed
 * to fall back to the serial decoder when they see tokens that need global
 * parser context, such as raster resets or palette redefinitions.
 */

typedef struct sixel_decoder_thread_config {
    int override_active;
    int override_threads;
} sixel_decoder_thread_config_t;

#if SIXEL_ENABLE_THREADS
typedef enum sixel_decoder_direct_mode {
    SIXEL_DECODER_DIRECT_PARSE = 0,
    SIXEL_DECODER_DIRECT_GATHER = 1
} sixel_decoder_direct_mode_t;

typedef struct sixel_decoder_worker_context {
    struct sixel_decoder_worker_chain *chain;
    unsigned char *input;
    unsigned char *anchor;
    int length;
    int payload_len;
    int start_offset;
    int end_offset;
    int index;
    int direct_mode;
    int initial_color_index;
    /* Parse results and local columns survive until the gather joins.
     * Byte-span counts exclude alignment; empty aligned spans own no bands. */
    image_buffer_t *image;
    uint64_t *columns;
    int column_bands;
    int column_capacity;
    int fail_columns;
    int validate_only;
    int trailing_data;
    int body_offset;
    int span_newlines;
    int boundary_newlines;
    int row_offset;
    int last_set_row;
    int decode_offset;
    int decode_color_index;
    int const *palette;
    int palette_limit;
    int width;
    int height;
    int pixel_size;
    int depth;
    sixel_timeline_logger_t *logger;
    int trust_raster_size;
    int painted_outside_raster;
    int first_painted_row;
    int last_painted_row;
    int max_color_index;
    int result;
    int runtime_error;
    sixel_decoder_direct_mode_t direct_mode_kind;
    sixel_decoder_undither_context_t *undither;
    int body_local_start;
} sixel_decoder_worker_context_t;

typedef struct sixel_decoder_local_chunk {
    unsigned char *data;
    unsigned char *paint_mask;
    int start_row;
    int rows;
    struct sixel_decoder_local_chunk *next;
} sixel_decoder_local_chunk_t;

typedef struct sixel_local_buffer {
    sixel_decoder_local_chunk_t *head;
    sixel_decoder_local_chunk_t *tail;
    sixel_decoder_local_chunk_t *cursor;
    int width;
    int pixel_size;
    int keep_paint_mask;
} sixel_local_buffer_t;

typedef struct sixel_decoder_worker_chain {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    int thread_count;
    int decode_done;
    int decode_failed;
    int paint_ready;
    int paint_released;
    unsigned char *global_buffer;
    unsigned char *global_mask;
    int global_capacity;
    int pixel_size;
    int abort_requested;
} sixel_decoder_worker_chain_t;
#endif

static sixel_decoder_thread_config_t g_decoder_threads = {
    0,
    1
};

#if SIXEL_ENABLE_THREADS
static sixel_mutex_t g_decoder_threads_mutex;
static int g_decoder_threads_mutex_ready;

# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
#  if !defined(UNICODE)
#   define UNICODE
#  endif
#  if !defined(_UNICODE)
#   define _UNICODE
#  endif
#  if !defined(WIN32_LEAN_AND_MEAN)
#   define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
static INIT_ONCE g_decoder_threads_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
sixel_decoder_threads_lock_init_once(PINIT_ONCE once,
                                      PVOID parameter,
                                      PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;

    if (sixel_mutex_init(&g_decoder_threads_mutex) == SIXEL_OK) {
        g_decoder_threads_mutex_ready = 1;
    }
    return TRUE;
}
# else
#  include <pthread.h>
SIXEL_PTHREAD_ONCE_DECLARE(g_decoder_threads_once);

static void
sixel_decoder_threads_lock_init_once(void)
{
    if (sixel_mutex_init(&g_decoder_threads_mutex) == SIXEL_OK) {
        g_decoder_threads_mutex_ready = 1;
    }
}
# endif

static void
sixel_decoder_threads_lock(void)
{
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    BOOL initialized;

    initialized = InitOnceExecuteOnce(&g_decoder_threads_once,
                                      sixel_decoder_threads_lock_init_once,
                                      NULL,
                                      NULL);
    if (!initialized || !g_decoder_threads_mutex_ready) {
        abort();
    }
# else
    int once_status;

    once_status = pthread_once(&g_decoder_threads_once,
                               sixel_decoder_threads_lock_init_once);
    if (once_status != 0 || !g_decoder_threads_mutex_ready) {
        abort();
    }
# endif
    sixel_mutex_lock(&g_decoder_threads_mutex);
}

static void
sixel_decoder_threads_unlock(void)
{
    if (!g_decoder_threads_mutex_ready) {
        abort();
    }
    sixel_mutex_unlock(&g_decoder_threads_mutex);
}
#else
static void
sixel_decoder_threads_lock(void)
{
}

static void
sixel_decoder_threads_unlock(void)
{
}
#endif

#if SIXEL_ENABLE_THREADS
static void
sixel_decoder_parallel_fill_spans(int payload_len,
                                  int threads,
                                  int *spans);
#endif /* SIXEL_ENABLE_THREADS */

int
sixel_decoder_parallel_skew_percent(void)
{
    /*
     * SIXEL_PARALLEL_SKEW lets operators bias span lengths by +/-20% so the
     * trailing workers take slightly more work.  The default keeps spans
     * balanced.
     */
    return sixel_runtime_policy_parallel_skew(0);
}

#if SIXEL_ENABLE_THREADS
static void
sixel_decoder_parallel_fill_spans(int payload_len,
                                  int threads,
                                  int *spans)
{
    int base_share;
    int skew_percent;
    double skew;
    int i;
    double center;
    int total;
    int remainder;

    base_share = payload_len / threads;
    skew_percent = sixel_decoder_parallel_skew_percent();
    skew = ((double)base_share * (double)skew_percent) / 100.0;
    total = 0;
    for (i = 0; i < threads; ++i) {
        center = (double)i - (double)(threads - 1) / 2.0;
        spans[i] = base_share + (int)(skew * center);
        if (spans[i] < 1) {
            spans[i] = 1;
        }
        total += spans[i];
    }

    remainder = payload_len - total;
    while (remainder > 0) {
        for (i = threads - 1; i >= 0 && remainder > 0; --i) {
            spans[i] += 1;
            --remainder;
        }
    }
    while (remainder < 0) {
        for (i = threads - 1; i >= 0 && remainder < 0; --i) {
            if (spans[i] > 1) {
                spans[i] -= 1;
                ++remainder;
            }
        }
        if (remainder < 0) {
            break;
        }
    }
}

static int
sixel_decoder_parallel_pixel_size(int depth)
{
    int pixel_size;

    pixel_size = 4;
    if (depth == 1) {
        pixel_size = 1;
    } else if (depth == 2) {
        pixel_size = 2;
    }

    return pixel_size;
}

static void
sixel_decoder_parallel_store_pixel(unsigned char *dst,
                                   int depth,
                                   int color_index,
                                   int const *palette)
{
    int color;

    if (depth == 1) {
        dst[0] = (unsigned char)color_index;
        return;
    }

    if (depth == 2) {
        unsigned short packed;

        packed = (unsigned short)color_index;
        memcpy(dst, &packed, sizeof(packed));
        return;
    }

    color = 0;
    if (palette != NULL &&
            color_index >= 0 &&
            color_index < SIXEL_PALETTE_MAX_DECODER) {
        color = palette[color_index];
    }
    dst[0] = (unsigned char)((color >> 16) & 0xff);
    dst[1] = (unsigned char)((color >> 8) & 0xff);
    dst[2] = (unsigned char)(color & 0xff);
    dst[3] = 255u;
}

static void
sixel_local_buffer_init(sixel_local_buffer_t *buffer,
                        int width,
                        int pixel_size,
                        int keep_paint_mask)
{
    buffer->head = NULL;
    buffer->tail = NULL;
    buffer->cursor = NULL;
    buffer->width = width;
    buffer->pixel_size = pixel_size;
    buffer->keep_paint_mask = keep_paint_mask != 0 ? 1 : 0;
}

static void
sixel_local_buffer_dispose(sixel_local_buffer_t *buffer)
{
    sixel_decoder_local_chunk_t *cursor;

    cursor = buffer->head;
    while (cursor != NULL) {
        sixel_decoder_local_chunk_t *tmp;

        free(cursor->data);
        free(cursor->paint_mask);
        tmp = cursor->next;
        free(cursor);
        cursor = tmp;
    }

    buffer->head = NULL;
    buffer->tail = NULL;
    buffer->cursor = NULL;
}

static sixel_decoder_local_chunk_t *
sixel_local_buffer_append(sixel_local_buffer_t *buffer,
                          int rows,
                          int start_row)
{
    sixel_decoder_local_chunk_t *chunk;
    size_t bytes;

    if (rows % 6 != 0) {
        rows += 6 - (rows % 6);
    }

    chunk = (sixel_decoder_local_chunk_t *)calloc(
        1, sizeof(sixel_decoder_local_chunk_t));
    if (chunk == NULL) {
        return NULL;
    }

    bytes = (size_t)(rows * buffer->width) * (size_t)buffer->pixel_size;
    chunk->data = (unsigned char *)calloc(bytes, 1);
    if (chunk->data == NULL) {
        free(chunk);
        return NULL;
    }
    if (buffer->keep_paint_mask) {
        bytes = (size_t)(rows * buffer->width);
        chunk->paint_mask = (unsigned char *)calloc(bytes, 1);
        if (chunk->paint_mask == NULL) {
            free(chunk->data);
            free(chunk);
            return NULL;
        }
    }

    chunk->start_row = start_row;
    chunk->rows = rows;
    chunk->next = NULL;

    if (buffer->head == NULL) {
        buffer->head = chunk;
    } else {
        buffer->tail->next = chunk;
    }
    buffer->tail = chunk;
    buffer->cursor = chunk;

    return chunk;
}

static sixel_decoder_local_chunk_t *
sixel_local_buffer_reserve_row(sixel_local_buffer_t *buffer,
                               int row)
{
    sixel_decoder_local_chunk_t *cursor;

    cursor = buffer->cursor;
    while (cursor != NULL && row >= cursor->start_row + cursor->rows) {
        cursor = cursor->next;
    }
    if (cursor != NULL && row >= cursor->start_row) {
        buffer->cursor = cursor;
        return cursor;
    }

    cursor = buffer->tail;
    if (cursor == NULL) {
        return NULL;
    }

    while (row >= cursor->start_row + cursor->rows) {
        sixel_decoder_local_chunk_t *next;
        int start_row;

        start_row = cursor->start_row + cursor->rows;
        next = sixel_local_buffer_append(buffer, cursor->rows, start_row);
        if (next == NULL) {
            return NULL;
        }
        cursor = next;
    }

    buffer->cursor = cursor;
    return cursor;
}

static unsigned char *
sixel_decoder_parallel_find_fast4_halo(unsigned char *anchor,
                                       unsigned char *body)
{
    unsigned char *scan;

    if (anchor == NULL || body == NULL || body <= anchor) {
        return body;
    }
    if (body[-1] != '-') {
        return body;
    }

    scan = body - 1;
    while (scan > anchor) {
        --scan;
        if (*scan == '-') {
            return scan + 1;
        }
    }

    return anchor;
}

static int
sixel_decoder_parallel_add_line_count(int *line_count)
{
    /*
     * Row anchors are stored as int pixel offsets. Reject a band count before
     * multiplying it by the six-pixel band height would overflow.
     */
    if (line_count == NULL || *line_count >= INT_MAX / 6) {
        return (-1);
    }

    *line_count += 1;
    return 0;
}

static int
sixel_decoder_parallel_count_newlines(unsigned char const *begin,
                                      unsigned char const *end)
{
    unsigned char const *scan;
    int lines;

    lines = 0;
    scan = begin;
    while (scan != NULL && scan < end) {
        scan = (unsigned char const *)memchr(scan, '-', (size_t)(end - scan));
        if (scan == NULL) {
            break;
        }
        if (sixel_decoder_parallel_add_line_count(&lines) != 0) {
            return (-1);
        }
        ++scan;
    }

    return lines;
}

/* Only a color command can contain '#'. Search for the nearest selection,
 * then parse its digits forward with the same bound as the body parser.
 * A palette definition also selects its register; the owning scan rejects
 * the definition itself before any shared destination can be written. */
static int
sixel_decoder_parallel_initial_color(unsigned char const *anchor,
                                      unsigned char const *cursor,
                                      int initial, int *color)
{
    unsigned char const *scan;
    unsigned char const *p;
    int value;

    *color = initial < 0 ? 0 : initial;
    if (*color >= SIXEL_PALETTE_MAX_DECODER) {
        *color = SIXEL_PALETTE_MAX_DECODER - 1;
    }
    scan = cursor;
    while (scan > anchor) {
        --scan;
        if (*scan != '#') {
            continue;
        }
        value = 0;
        p = scan + 1;
        while (p < cursor && *p >= '0' && *p <= '9') {
            if (value > SIXEL_PALETTE_MAX_DECODER / 10 ||
                    (value == SIXEL_PALETTE_MAX_DECODER / 10 &&
                    (*p - '0') > SIXEL_PALETTE_MAX_DECODER % 10)) {
                return (-1);
            }
            value = value * 10 + (*p - '0');
            ++p;
        }
        *color = value >= SIXEL_PALETTE_MAX_DECODER ?
            SIXEL_PALETTE_MAX_DECODER - 1 : value;
        break;
    }
    return 0;
}

static void
sixel_decoder_parallel_cancel_decode(sixel_decoder_worker_chain_t *chain)
{
    if (chain == NULL) {
        return;
    }

    sixel_mutex_lock(&chain->mutex);
    chain->decode_failed = 1;
    chain->abort_requested = 1;
    sixel_cond_broadcast(&chain->cond);
    sixel_mutex_unlock(&chain->mutex);
}

static int
sixel_decoder_parallel_finish_decode(sixel_decoder_worker_chain_t *chain,
                                     int status)
{
    int publish_ready;

    if (chain == NULL) {
        return (-1);
    }

    publish_ready = 0;
    sixel_mutex_lock(&chain->mutex);
    chain->decode_done += 1;
    if (status != 0) {
        chain->decode_failed = 1;
        chain->abort_requested = 1;
    }
    sixel_cond_broadcast(&chain->cond);

    /*
     * Non-fast4 workers publish into the shared image only after every
     * worker has finished decoding successfully.  A failed worker marks the
     * chain aborted and wakes peers so request_start() can return
     * SIXEL_FALSE without dirtying the serial fallback image.
     */
    while (!chain->abort_requested &&
            chain->decode_done < chain->thread_count) {
        sixel_cond_wait(&chain->cond, &chain->mutex);
    }
    if (!chain->abort_requested && !chain->decode_failed &&
            chain->decode_done >= chain->thread_count) {
        publish_ready = 1;
    }

    sixel_mutex_unlock(&chain->mutex);
    return publish_ready ? 0 : (-1);
}

/* Grow only the relative bands that could still intersect the canvas after
 * prefix resolution. Clipped trailing bands need extents, not storage. */
static int
sixel_decoder_parallel_reserve_columns(sixel_decoder_worker_context_t *context,
                                       int band)
{
    uint64_t *columns;
    size_t count;
    size_t old_count;
    int capacity;
    int limit;

    if (context->fail_columns) {
        return (-1);
    }
    limit = (context->height + 5) / 6;
    if (band >= limit || band < context->column_capacity) {
        return 0;
    }
    capacity = context->column_capacity > 0 ? context->column_capacity : 8;
    while (capacity <= band) {
        if (capacity > limit / 2) {
            capacity = limit;
            break;
        }
        capacity *= 2;
    }
    if (capacity > limit) {
        capacity = limit;
    }
    if ((size_t)capacity > SIZE_MAX / sizeof(uint64_t) /
            (size_t)context->width) {
        return (-1);
    }
    count = (size_t)capacity * (size_t)context->width;
    old_count = (size_t)context->column_capacity * (size_t)context->width;
    columns = (uint64_t *)realloc(context->columns, count * sizeof(uint64_t));
    if (columns == NULL) {
        return (-1);
    }
    memset(columns + old_count, 0, (count - old_count) * sizeof(uint64_t));
    context->columns = columns;
    context->column_capacity = capacity;
    return 0;
}

/* Highest and lowest set row of a six-bit sixel pattern (index 0 unused). */
static signed char const sixel_parallel_top_row[64] = {
    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
};
static signed char const sixel_parallel_low_row[64] = {
    -1, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};

/* Parse the body once. Ordinary workers retain band columns locally;
 * undither requests retain the validation-only pass because their later
 * chunk parser also needs the preceding band's reconstruction halo. */
static int
sixel_decoder_parallel_direct_parse(sixel_decoder_worker_context_t *context)
{
    unsigned char *anchor;
    unsigned char *cursor;
    unsigned char *stop;
    unsigned char *limit;
    unsigned char *start;
    unsigned char *p;
    unsigned char ch;
    uint64_t *columns;
    uint64_t color;
    uint64_t mask;
    int bits;
    int rows;
    int repeat;
    int color_index;
    int pos_x;
    int pos_y;
    int value;
    int effective_repeat;
    int width;
    int starts_after_newline;
    int status;
    int r;
    int retain;
    int columns_band;
    int first_painted_row;
    int last_painted_row;
    int last_set_row;
    int max_color_index;

    anchor = context->anchor;
    width = context->width;
    repeat = 1;
    color_index = context->initial_color_index;
    pos_x = 0;
    pos_y = 0;
    status = (-1);
    retain = context->undither == NULL && !context->validate_only;
    context->painted_outside_raster = 0;
    /* Paint extents stay in locals while parsing and are published at end
     * on every exit, as the context fields previously were. */
    first_painted_row = (-1);
    last_painted_row = (-1);
    last_set_row = (-1);
    max_color_index = (-1);

    if (context->logger != NULL) {
        sixel_timeline_logger_logf(context->logger,
            retain ? "parse" : "scan", "decoder", "start", context->index);
    }
    if (context->input == NULL || anchor == NULL ||
            context->length <= 0 || context->payload_len <= 0 ||
            width <= 0 || context->height <= 0) {
        goto end;
    }
    start = context->input + context->start_offset;
    stop = context->input + context->end_offset;
    limit = context->input + context->length;
    cursor = start;
    if (context->index > 0) {
        starts_after_newline = start > anchor && start[-1] == '-';
        if (!starts_after_newline) {
            cursor = (unsigned char *)memchr(start, '-',
                                             (size_t)(limit - start));
            if (cursor == NULL) {
                goto end;
            }
            ++cursor;
        }
    }
    if (!retain && cursor == limit) {
        /* The legacy validation path rejects alignment past the last byte. */
        goto end;
    }
    /* Count original spans while parsing their owned bands. Alignment skips
     * at most one delimiter inside this span; a delimiter beyond its end is
     * accounted for only by the boundary adjustment, not counted twice. */
    context->span_newlines = cursor > start && cursor <= stop + 1 ? 1 : 0;
    context->boundary_newlines = cursor > start ? 1 : 0;
    if (!retain) {
        /* The halo path keeps its established original-span prefix counts. */
        context->span_newlines = sixel_decoder_parallel_count_newlines(
            start, stop + 1);
        context->boundary_newlines = cursor > start ? 1 : 0;
    }
    context->body_offset = (int)(cursor - context->input);
    context->decode_offset = context->body_offset;
    context->decode_color_index = color_index;
    if (context->span_newlines < 0 ||
            sixel_decoder_parallel_initial_color(anchor, cursor,
                color_index, &color_index) != 0) {
        goto end;
    }
    context->initial_color_index = color_index;
    context->body_local_start = 0;
    if (context->undither != NULL) {
        p = sixel_decoder_parallel_find_fast4_halo(anchor, cursor);
        context->body_local_start =
            sixel_decoder_parallel_count_newlines(p, cursor);
        if (context->body_local_start < 0 ||
                sixel_decoder_parallel_initial_color(anchor, p,
                    context->decode_color_index,
                    &context->decode_color_index) != 0) {
            goto end;
        }
        context->body_local_start *= 6;
        context->decode_offset = (int)(p - context->input);
    } else if (retain) {
        /* A boundary beyond this original span belongs to a later worker.
         * In particular, converging starts must not decode that band twice. */
        if (cursor > stop) {
            status = 0;
            goto end;
        }
        if (color_index >= 256) {
            goto end;
        }
    }
    color = sixel_column_color((unsigned int)color_index);
    /* The column storage of a band changes only when the band does: reserve
     * and locate it once per band instead of once per run. */
    columns = NULL;
    columns_band = (-1);
    while (cursor < limit) {
        ch = *cursor;
        if (ch >= '?' && ch <= '~') {
            rows = 0;
            effective_repeat = repeat;
            if (pos_x > INT_MAX - repeat) {
                goto end;
            }
            if (retain && pos_y / 6 != columns_band) {
                if (sixel_decoder_parallel_reserve_columns(context,
                                                           pos_y / 6) != 0) {
                    goto end;
                }
                columns_band = pos_y / 6;
                columns = columns_band < context->column_capacity ?
                    context->columns +
                        (size_t)columns_band * (size_t)width : NULL;
            }
            /* One word update per literal; aggregate the six row bits once
             * per run, preserving unset-column cursor movement. */
            if (retain && repeat == 1 && pos_x < width && columns != NULL) {
                do {
                    bits = *cursor++ - '?';
                    mask = sixel_column_row_masks[bits];
                    columns[pos_x] = (columns[pos_x] & ~mask) |
                        (color & mask) | ((uint64_t)bits << 48);
                    rows |= bits;
                    ++pos_x;
                } while (cursor < limit && *cursor >= '?' && *cursor <= '~' &&
                         pos_x < width);
            } else {
                bits = ch - '?';
                rows = bits;
                if (pos_x + repeat > width && bits != 0) {
                    if (!context->trust_raster_size) {
                        goto end;
                    }
                    context->painted_outside_raster = 1;
                }
                if (effective_repeat > width - pos_x) {
                    effective_repeat = pos_x < width ? width - pos_x : 0;
                }
                if (bits != 0 && effective_repeat > 0 && columns != NULL) {
                    sixel_column_store(columns + pos_x, color, bits,
                                       effective_repeat);
                }
                pos_x += repeat;
                repeat = 1;
                ++cursor;
            }
            if (rows != 0) {
                r = sixel_parallel_top_row[rows];
                if (pos_y > INT_MAX - r) {
                    goto end;
                }
                if (pos_y + r > last_set_row) {
                    last_set_row = pos_y + r;
                }
                if (effective_repeat > 0) {
                    if (last_set_row > last_painted_row) {
                        last_painted_row = last_set_row;
                    }
                    r = sixel_parallel_low_row[rows];
                    if (first_painted_row < 0 ||
                            pos_y + r < first_painted_row) {
                        first_painted_row = pos_y + r;
                    }
                    if (color_index > max_color_index) {
                        max_color_index = color_index;
                    }
                }
            }
            continue;
        }
        if (ch == '#' || ch == '!') {
            value = 0;
            p = cursor + 1;
            while (p < limit && *p >= '0' && *p <= '9') {
                if (value > 6553 || (value == 6553 &&
                        (*p - '0') > (ch == '#' ? 6 : 5))) {
                    goto end;
                }
                value = value * 10 + (*p++ - '0');
            }
            if (ch == '#') {
                if ((p < limit && *p == ';') || (retain && value >= 256)) {
                    goto end;
                }
                color_index = value >= SIXEL_PALETTE_MAX_DECODER ?
                    SIXEL_PALETTE_MAX_DECODER - 1 : value;
                color = sixel_column_color((unsigned int)color_index);
            } else {
                repeat = value > 0 ? value : 1;
            }
            cursor = p;
            continue;
        }
        if (ch == '$') {
            ++cursor;
            pos_x = 0;
            continue;
        }
        if (ch == '-') {
            if (retain && cursor <= stop &&
                    sixel_decoder_parallel_add_line_count(
                        &context->span_newlines) != 0) {
                goto end;
            }
            if (cursor >= stop) {
                break;
            }
            ++cursor;
            pos_x = 0;
            if (pos_y > INT_MAX - 6) {
                goto end;
            }
            pos_y += 6;
            continue;
        }
        if (ch < 0x20) {
            if (ch == 0x18 || ch == 0x1a) {
                goto end;
            }
            ++cursor;
            if (ch == 0x1b && cursor < limit && *cursor == '\\') {
                context->trailing_data = cursor + 1 < limit;
                /* Preserve legacy byte-span offsets even when malformed
                 * trailing data follows ST inside a span. Only the ignored
                 * tail needs a delimiter count; it is never interpreted. */
                if (retain && cursor <= stop) {
                    value = sixel_decoder_parallel_count_newlines(cursor,
                                                                  stop + 1);
                    if (value < 0 || context->span_newlines >
                            INT_MAX / 6 - value) {
                        goto end;
                    }
                    context->span_newlines += value;
                }
                break;
            }
            continue;
        }
        if (ch == '"') {
            goto end;
        }
        ++cursor;
    }
    if (retain) {
        context->column_bands = pos_y / 6 + 1;
        if (context->column_bands > context->column_capacity) {
            context->column_bands = context->column_capacity;
        }
    }
    status = 0;
end:
    context->first_painted_row = first_painted_row;
    context->last_painted_row = last_painted_row;
    context->last_set_row = last_set_row;
    context->max_color_index = max_color_index;
    if (context->logger != NULL) {
        sixel_timeline_logger_logf(context->logger,
            retain ? "parse" : "scan", "decoder",
            status == 0 ? "finish" : "abort", context->index);
    }
    return status;
}

static int
sixel_decoder_parallel_direct_worker(void *arg)
{
    sixel_decoder_worker_context_t *context;
    sixel_decoder_worker_chain_t *chain;
    int first;
    int end;
    int status;

    context = (sixel_decoder_worker_context_t *)arg;
    chain = context->chain;
    if (context->direct_mode_kind == SIXEL_DECODER_DIRECT_PARSE) {
        status = sixel_decoder_parallel_direct_parse(context);
    } else {
        if (context->logger != NULL) {
            sixel_timeline_logger_logf(context->logger,
                "gather", "decoder", "ready", context->index);
        }
        sixel_mutex_lock(&chain->mutex);
        chain->paint_ready += 1;
        sixel_cond_broadcast(&chain->cond);
        while (!chain->paint_released && !chain->abort_requested) {
            sixel_cond_wait(&chain->cond, &chain->mutex);
        }
        status = chain->abort_requested ? (-1) : 0;
        sixel_mutex_unlock(&chain->mutex);
        if (status == 0) {
            if (context->logger != NULL) {
                sixel_timeline_logger_logf(context->logger,
                    "gather", "decoder", "start", context->index);
            }
            first = context->row_offset / 6;
            end = first + context->column_bands;
            if (end > (context->height + 5) / 6) {
                end = (context->height + 5) / 6;
            }
            image_buffer_finalize_normal_columns(context->image,
                context->columns, first, end);
            if (context->logger != NULL) {
                sixel_timeline_logger_logf(context->logger,
                    "gather", "decoder", "finish", context->index);
            }
        }
    }
    context->result = status;
    return status;
}

/* Prefix counts cover original disjoint byte spans, plus the delimiter
 * skipped for alignment. Ordinary workers count these during their sole
 * parse; undither retains its validation-only scan and halo adjustment. */
static int
sixel_decoder_parallel_resolve_rows(sixel_decoder_worker_context_t *contexts,
                                    int threads)
{
    sixel_decoder_worker_context_t *context;
    int lines;
    int offset;
    int i;

    lines = 0;
    for (i = 0; i < threads; ++i) {
        context = &contexts[i];
        if (lines > INT_MAX / 6 - context->boundary_newlines) {
            return 0;
        }
        offset = (lines + context->boundary_newlines) * 6;
        context->row_offset = offset;
        if (context->body_local_start > offset ||
                (context->last_set_row >= 0 &&
                 offset > INT_MAX - context->last_set_row)) {
            return 0;
        }
        if (context->last_set_row >= 0 &&
                offset + context->last_set_row >= context->height) {
            if (!context->trust_raster_size) {
                return 0;
            }
            context->painted_outside_raster = 1;
        }
        if (context->first_painted_row >= 0) {
            context->first_painted_row += offset;
            context->last_painted_row += offset;
            if (context->first_painted_row >= context->height) {
                context->first_painted_row = (-1);
                context->last_painted_row = (-1);
            } else if (context->last_painted_row >= context->height) {
                context->last_painted_row = context->height - 1;
            }
        }
        if (lines > INT_MAX / 6 - context->span_newlines) {
            return 0;
        }
        lines += context->span_newlines;
    }
    return 1;
}

/* Legacy byte splitting can paint trailing bytes after an embedded ST, or
 * fall back if aligned starts overlap. Keep that existing malformed-input
 * behavior at the same worker budget. Only this exceptional case needs to
 * reconstruct the formerly duplicated first band of an empty aligned span;
 * well-formed input remains a single parse with no row-overlap pass. */
static int
sixel_decoder_parallel_tail_compatible(
    sixel_decoder_worker_context_t const *contexts, int threads)
{
    sixel_decoder_worker_context_t *legacy;
    int trailing;
    int empty;
    int previous;
    int ok;
    int i;

    trailing = 0;
    empty = 0;
    for (i = 0; i < threads; ++i) {
        trailing |= contexts[i].trailing_data;
        empty |= contexts[i].body_offset > contexts[i].end_offset;
    }
    if (!trailing || !empty) {
        return 1;
    }
    legacy = (sixel_decoder_worker_context_t *)malloc(
        (size_t)threads * sizeof(*legacy));
    if (legacy == NULL) {
        return 0;
    }
    memcpy(legacy, contexts, (size_t)threads * sizeof(*legacy));
    ok = 0;
    for (i = 0; i < threads; ++i) {
        if (legacy[i].body_offset > legacy[i].end_offset) {
            legacy[i].validate_only = 1;
            legacy[i].logger = NULL;
            if (sixel_decoder_parallel_direct_parse(&legacy[i]) != 0) {
                goto end;
            }
        }
    }
    if (!sixel_decoder_parallel_resolve_rows(legacy, threads)) {
        goto end;
    }
    previous = (-1);
    for (i = 0; i < threads; ++i) {
        if (legacy[i].first_painted_row >= 0) {
            if (legacy[i].first_painted_row <= previous) {
                goto end;
            }
            previous = legacy[i].last_painted_row;
        }
    }
    ok = 1;
end:
    free(legacy);
    return ok;
}

static int
sixel_decoder_parallel_inject_failure(char const *phase, int index)
{
    char const *text;
    char *end;
    long value;
    size_t length;

    text = sixel_test_environment_decoder_parallel_failure();
    end = NULL;
    value = (-1);
    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    length = strlen(phase);
    if (strncmp(text, phase, length) != 0 || text[length] != ':') {
        return 0;
    }
    text += length + 1;
    value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 0 || value > INT_MAX) {
        return 0;
    }

    return value == index;
}

#if !defined(SIXEL_DECODE_PIXELS_NO_FAST4)
static int
sixel_decoder_parallel_copy_local_rows(sixel_local_buffer_t *buffer,
                                       unsigned char *indexed,
                                       int rows)
{
    sixel_decoder_local_chunk_t *chunk;
    int copy_rows;
    size_t offset;
    size_t bytes;

    if (buffer == NULL || indexed == NULL || rows <= 0 ||
            buffer->pixel_size != 1) {
        return 0;
    }

    chunk = buffer->head;
    while (chunk != NULL) {
        copy_rows = chunk->rows;
        if (chunk->start_row >= rows) {
            copy_rows = 0;
        } else if (chunk->start_row + copy_rows > rows) {
            copy_rows = rows - chunk->start_row;
        }
        if (copy_rows > 0) {
            offset = (size_t)chunk->start_row * (size_t)buffer->width;
            bytes = (size_t)copy_rows * (size_t)buffer->width;
            memcpy(indexed + offset, chunk->data, bytes);
        }
        chunk = chunk->next;
    }

    return 1;
}

static int
sixel_decoder_parallel_write_fast4_rows(
    sixel_decoder_worker_context_t *context,
    sixel_local_buffer_t *buffer,
    int row_offset,
    int local_rows,
    int max_color_index)
{
    sixel_decoder_undither_context_t *undither;
    unsigned char *indexed;
    unsigned char *rgb;
    unsigned char *src;
    unsigned char *dst;
    size_t pixels;
    size_t rgb_bytes;
    size_t out_offset;
    int body_start;
    int y;
    int x;
    int width;
    int height;
    int global_y;
    int status;
    int active_ncolors;

    if (context == NULL || buffer == NULL || context->undither == NULL) {
        return (-1);
    }

    undither = context->undither;
    width = context->width;
    height = context->height;
    body_start = context->body_local_start;
    if (!undither->enabled || undither->pixels == NULL ||
            undither->palette == NULL || width <= 0 || height <= 0 ||
            local_rows <= body_start || body_start < 0) {
        return 0;
    }
    if (row_offset + body_start >= height) {
        return 0;
    }
    if (row_offset + local_rows > height) {
        local_rows = height - row_offset;
    }
    if (local_rows <= body_start) {
        return 0;
    }

    active_ncolors = undither->ncolors;
    if (max_color_index >= 0 && active_ncolors < max_color_index + 1) {
        active_ncolors = max_color_index + 1;
    }
    if (active_ncolors <= 0 || active_ncolors > undither->palette_size) {
        return (-1);
    }

    pixels = (size_t)local_rows * (size_t)width;
    if (pixels > ((size_t)-1 / 3u)) {
        return (-1);
    }
    rgb_bytes = pixels * 3u;
    indexed = (unsigned char *)calloc(pixels, 1);
    rgb = (unsigned char *)calloc(rgb_bytes, 1);
    if (indexed == NULL || rgb == NULL) {
        free(indexed);
        free(rgb);
        return (-1);
    }

    if (!sixel_decoder_parallel_copy_local_rows(buffer, indexed,
                                                local_rows)) {
        free(indexed);
        free(rgb);
        return (-1);
    }

    status = sixel_dequantize_k_undither_fast4_rows(
        indexed,
        width,
        local_rows,
        undither->palette,
        active_ncolors,
        undither->similarity_bias,
        body_start,
        local_rows,
        undither->allocator,
        rgb);
    if (SIXEL_FAILED(status)) {
        free(indexed);
        free(rgb);
        return (-1);
    }

    for (y = body_start; y < local_rows; ++y) {
        global_y = row_offset + y;
        if (global_y < 0 || global_y >= height) {
            continue;
        }
        src = rgb + (size_t)y * (size_t)width * 3u;
        out_offset = (size_t)global_y * (size_t)width *
            (size_t)undither->pixel_size;
        dst = undither->pixels + out_offset;
        if (undither->direct_output) {
            for (x = 0; x < width; ++x) {
                dst[x * 4 + 0] = src[x * 3 + 0];
                dst[x * 4 + 1] = src[x * 3 + 1];
                dst[x * 4 + 2] = src[x * 3 + 2];
                dst[x * 4 + 3] = 255u;
            }
        } else {
            memcpy(dst, src, (size_t)width * 3u);
        }
    }

    free(indexed);
    free(rgb);
    return 0;
}
#endif

/*
 * Local-buffer parser entry. The validation pass has cached this worker's
 * aligned body, top halo, selected register and absolute row offset. Walk
 * tokens until the first '-' after the scheduled end offset.
 *
 * Supported tokens: palette switches ('#n'), carriage return ('$'), next
 * line ('-'), and raster data ('?' - '~').  Any raster attribute reset '"',
 * palette redefinition '#n;type;...', or CAN/SUB control code marks the
 * chunk as invalid and returns a failure status so the caller can fall back.
 */
static int
sixel_decoder_parallel_worker(void *arg)
{
    sixel_decoder_worker_context_t *context;
    sixel_decoder_worker_chain_t *chain;
    sixel_local_buffer_t local_buffer;
    sixel_decoder_local_chunk_t *chunk_cursor = NULL;
    unsigned char *cursor = NULL;
    unsigned char *stop = NULL;
    unsigned char *limit = NULL;
    unsigned char *start = NULL;
    int capacity = 0;
    int written = 0;
    int assigned = 0;
    int bits = 0;
    int repeat = 1;
    int color_index = 0;
    int max_relative = (-1);
    int min_relative = (-1);
    int pos_x = 0;
    int pos_y = 0;
    int relative = 0;
    int row_base = 0;
    int r = 0;
    int row_offset = 0;
    int i;
    int fallback = 0;
    int status = (-1);
    sixel_timeline_logger_t *logger = NULL;
    int width = 0;
    int pixel_size = 0;
    int depth = 0;
    int height = 0;
    int max_color_index = (-1);
    int effective_repeat = 0;
    int global_y = 0;
    int local_rows = 0;
    int publish_start = 0;
    int publish_end = 0;
    int rows;
    size_t chunk_bytes;
    size_t chunk_offset;
    unsigned char ch;
    unsigned char *p;
    int value;

    context = (sixel_decoder_worker_context_t *)arg;
    if (context == NULL) {
        return (-1);
    }

    context->result = status;
    context->max_color_index = (-1);
    context->painted_outside_raster = 0;

    chain = context->chain;
    if (chain == NULL) {
        context->result = status;
        return status;
    }

    logger = context->logger;
    if (context->input == NULL || context->length <= 0) {
        sixel_decoder_parallel_finish_decode(chain, status);
        context->result = status;
        return status;
    }

    width = context->width;
    if (width <= 0) {
        sixel_decoder_parallel_finish_decode(chain, status);
        context->result = status;
        return status;
    }

    pixel_size = context->pixel_size;
    depth = context->depth;
    height = context->height;
    if (height <= 0) {
        sixel_decoder_parallel_finish_decode(chain, status);
        context->result = status;
        return status;
    }
    sixel_local_buffer_init(&local_buffer,
                            width,
                            pixel_size,
                            chain->global_mask != NULL);
    if (context->payload_len <= 0) {
        sixel_decoder_parallel_finish_decode(chain, status);
        context->result = status;
        return status;
    }

    start = context->input + context->start_offset;
    if (start < context->input ||
            start >= context->input + context->length) {
        sixel_decoder_parallel_finish_decode(chain, status);
        context->result = status;
        return status;
    }

    assigned = context->end_offset - context->start_offset + 1;
    if (assigned <= 0) {
        sixel_decoder_parallel_finish_decode(chain, status);
        context->result = status;
        return status;
    }

    status = 0;

    if (logger != NULL) {
        /*
         * Decode window for this worker. The key keeps decode spans grouped
         * per-thread in the timeline output.
         */
        sixel_timeline_logger_logf(logger,
                          "decode",
                          "decoder",
                          "start",
                          context->index,
                          context->index,
                          0,
                          0,
                          context->start_offset,
                          context->end_offset,
                          "worker %d decode span [%d,%d]",
                          context->index,
                          context->start_offset,
                          context->end_offset);
    }

    /* The shared validation pass already found the body/halo boundary and
     * its selected register. Halo rows remain local filter input; they are
     * excluded from publication by body_local_start as before. */
    cursor = context->input + context->decode_offset;
    color_index = context->decode_color_index;
    row_offset = context->row_offset - context->body_local_start;

    capacity = (int)((double)chain->global_capacity *
        ((double)assigned / (double)context->payload_len));
    capacity = (int)((double)capacity * 1.10);
    if (capacity < 1) {
        capacity = 1;
    }
    if (capacity < width * 6) {
        capacity = width * 6;
    }
    if (capacity % (width * 6) != 0) {
        capacity += (width * 6) - (capacity % (width * 6));
    }

    chunk_cursor = sixel_local_buffer_append(&local_buffer,
                                             capacity / width,
                                             0);
    if (chunk_cursor == NULL) {
        status = (-1);
        sixel_decoder_parallel_finish_decode(chain, status);
        context->result = status;
        return status;
    }
    capacity = chunk_cursor->rows * width;

    stop = context->input + context->end_offset;
    limit = context->input + context->length;
    while (cursor < limit) {
        /*
         * Hot path prefers raster tokens ('?' - '~') to reduce branching.
         * Control and attribute tokens fall back to the slow path below.
         */
        ch = *cursor;

        /*
         * Branch ordering follows observed frequency:
         *   raster ('?' - '~') > '#' > '!' > '$' > '-'
         *   >>> control (< 0x20) > '"'.
         */
        if (ch >= '?' && ch <= '~') {
            bits = ch - '?';
            effective_repeat = repeat;
            if (pos_x < 0 || repeat < 0 ||
                    pos_x > INT_MAX - repeat) {
                fallback = 1;
                status = (-1);
                break;
            }
            if (context->trust_raster_size) {
                if (pos_x >= width) {
                    if (bits != 0) {
                        context->painted_outside_raster = 1;
                    }
                    effective_repeat = 0;
                } else if (pos_x + effective_repeat > width) {
                    if (bits != 0) {
                        context->painted_outside_raster = 1;
                    }
                    effective_repeat = width - pos_x;
                }
            }
            for (i = 0; i < 6; ++i) {
                if ((bits & (1 << i)) != 0) {
                    if (pos_y > INT_MAX - i ||
                            row_offset > INT_MAX - (pos_y + i)) {
                        fallback = 1;
                        status = (-1);
                        break;
                    }
                    global_y = row_offset + pos_y + i;
                    if (global_y >= height) {
                        if (context->trust_raster_size) {
                            context->painted_outside_raster = 1;
                            continue;
                        }
                        fallback = 1;
                        status = (-1);
                        break;
                    }
                    if (!context->trust_raster_size &&
                            pos_x + repeat > width) {
                        fallback = 1;
                        status = (-1);
                        break;
                    }
                    if (effective_repeat <= 0) {
                        continue;
                    }
                    chunk_cursor = sixel_local_buffer_reserve_row(
                        &local_buffer, pos_y + i);
                    if (chunk_cursor == NULL) {
                        fallback = 1;
                        status = (-1);
                        break;
                    }

                    row_base = (pos_y + i - chunk_cursor->start_row) *
                        width + pos_x;
                    /*
                     * Track the actual row touched by this sixel bit.  A
                     * single sixel byte may paint any of the six rows in the
                     * current band, and the copy stage truncates local chunks
                     * from this maximum touched position.
                     */
                    relative = (pos_y + i) * width + pos_x;

                    if (pixel_size == 1 && effective_repeat > 3) {
                        memset(chunk_cursor->data + (size_t)row_base,
                               color_index,
                               effective_repeat);
                        if (chunk_cursor->paint_mask != NULL) {
                            memset(chunk_cursor->paint_mask +
                                   (size_t)row_base,
                                   0xff,
                                   (size_t)effective_repeat);
                        }
                        if (color_index > max_color_index) {
                            max_color_index = color_index;
                        }
                    } else {
                        for (r = 0; r < effective_repeat; ++r) {
                            sixel_decoder_parallel_store_pixel(
                                chunk_cursor->data +
                                (size_t)(row_base + r) *
                                (size_t)pixel_size,
                                depth,
                                color_index,
                                context->palette);
                            if (chunk_cursor->paint_mask != NULL) {
                                chunk_cursor->paint_mask[row_base + r] =
                                    0xffu;
                            }
                        }
                        if (pixel_size == 1 &&
                                color_index > max_color_index) {
                            max_color_index = color_index;
                        }
                    }
                    written += effective_repeat;
                    if (min_relative < 0 || relative < min_relative) {
                        min_relative = relative;
                    }
                    if (max_relative < relative + effective_repeat - 1) {
                        max_relative = relative + effective_repeat - 1;
                    }
                }
            }

            if (fallback) {
                break;
            }

            cursor += 1;
            pos_x += repeat;
            repeat = 1;
            continue;
        }

        if (ch == '#') {
            value = 0;
            p = cursor + 1;
            while (p < limit && *p >= '0' && *p <= '9') {
                value = value * 10 + (*p - '0');
                p += 1;
            }
            if (p < limit && *p == ';') {
                fallback = 1;
                status = (-1);
                break;
            }
            color_index = value;
            if (color_index < 0) {
                color_index = 0;
            }
            if (color_index >= SIXEL_PALETTE_MAX_DECODER) {
                color_index = SIXEL_PALETTE_MAX_DECODER - 1;
            }
            cursor = p;
            continue;
        }

        if (ch == '!') {
            value = 0;
            p = cursor + 1;
            while (p < limit && *p >= '0' && *p <= '9') {
                value = value * 10 + (*p - '0');
                p += 1;
            }
            if (value <= 0) {
                value = 1;
            }
            repeat = value;
            cursor = p;
            continue;
        }

        if (ch == '$') {
            cursor += 1;
            pos_x = 0;
            continue;
        }

        if (ch == '-') {
            if (cursor >= stop) {
                break;
            }
            cursor += 1;
            pos_x = 0;
            if (pos_y > INT_MAX - 6) {
                fallback = 1;
                status = (-1);
                break;
            }
            pos_y += 6;
            chunk_cursor = sixel_local_buffer_reserve_row(&local_buffer,
                                                          pos_y);
            if (chunk_cursor == NULL) {
                fallback = 1;
                status = (-1);
                break;
            }
            continue;
        }

        if (ch < 0x20) {
            if (ch == 0x18 || ch == 0x1a) {
                fallback = 1;
                status = (-1);
                break;
            }
            cursor += 1;
            if (ch == 0x1b && cursor < limit && *cursor == '\\') {
                status = (0);
                break;
            }
            continue;
        }

        if (ch == '"') {
            fallback = 1;
            status = (-1);
            break;
        }

        cursor += 1;
    }

    local_rows = pos_y + 6;
    if (max_relative >= 0 && local_rows < (max_relative / width) + 1) {
        local_rows = (max_relative / width) + 1;
    }
    if (local_rows < context->body_local_start) {
        local_rows = context->body_local_start;
    }

    if (logger != NULL) {
        sixel_timeline_logger_logf(logger,
                          "decode",
                          "decoder",
                          fallback ? "abort" : "finish",
                          context->index,
                          context->index,
                          0,
                          0,
                          context->start_offset,
                          context->end_offset,
                          "worker %d decode wrote=%d status=%d",
                          context->index,
                          written,
                          status);
    }

    if (status != 0) {
        sixel_decoder_parallel_finish_decode(chain, status);
        sixel_local_buffer_dispose(&local_buffer);
        context->result = status;
        return status;
    }

#if !defined(SIXEL_DECODE_PIXELS_NO_FAST4)
    if (context->undither != NULL && context->undither->enabled) {
        status = sixel_decoder_parallel_write_fast4_rows(
            context,
            &local_buffer,
            row_offset,
            local_rows,
            max_color_index);
        if (status != 0) {
            sixel_decoder_parallel_cancel_decode(chain);
            sixel_local_buffer_dispose(&local_buffer);
            context->result = status;
            return status;
        }
        sixel_local_buffer_dispose(&local_buffer);
        context->max_color_index = max_color_index;
        context->result = status;
        return status;
    }
#endif

    status = sixel_decoder_parallel_finish_decode(chain, status);
    if (status != 0) {
        sixel_local_buffer_dispose(&local_buffer);
        status = (-1);
        context->result = status;
        return status;
    }

    if (max_relative >= 0) {
        publish_start = (row_offset * width + min_relative) * pixel_size;
        publish_end = (row_offset * width + max_relative + 1) * pixel_size;
    }
    if (logger != NULL) {
        sixel_timeline_logger_logf(logger,
                          "copy",
                          "decoder",
                          "start",
                          context->index,
                          context->index,
                          0,
                          0,
                          publish_start,
                          publish_end,
                          "worker %d publish rows=%d..%d",
                          context->index,
                          row_offset + (min_relative / width),
                          row_offset + (max_relative / width));
    }

    if (max_relative >= 0) {
        chunk_cursor = local_buffer.head;
        while (chunk_cursor != NULL) {
            rows = chunk_cursor->rows;
            if (chunk_cursor->start_row + rows >
                    (max_relative / width) + 1) {
                rows = (max_relative / width) + 1 -
                    chunk_cursor->start_row;
            }
            if (rows < 0) {
                rows = 0;
            }
            if (rows > 0) {
                chunk_bytes = (size_t)(rows * width) *
                    (size_t)chain->pixel_size;
                chunk_offset = (size_t)((row_offset +
                    chunk_cursor->start_row) * width) *
                    (size_t)chain->pixel_size;
                memcpy(chain->global_buffer + chunk_offset,
                       chunk_cursor->data,
                       chunk_bytes);
                if (chain->global_mask != NULL &&
                        chunk_cursor->paint_mask != NULL) {
                    chunk_bytes = (size_t)(rows * width);
                    chunk_offset = (size_t)(row_offset +
                        chunk_cursor->start_row) * (size_t)width;
                    memcpy(chain->global_mask + chunk_offset,
                           chunk_cursor->paint_mask,
                           chunk_bytes);
                }
            }
            chunk_cursor = chunk_cursor->next;
        }
    }

    if (logger != NULL) {
        sixel_timeline_logger_logf(logger,
                          "copy",
                          "decoder",
                          "finish",
                          context->index,
                          context->index,
                          0,
                          0,
                          publish_start,
                          publish_end,
                          "worker %d publish done",
                          context->index);
    }

    sixel_local_buffer_dispose(&local_buffer);

    context->max_color_index = max_color_index;
    context->result = status;
    return status;
}
#endif

static int
sixel_decoder_threads_normalize(int requested)
{
    int normalized;

#if SIXEL_ENABLE_THREADS
    int hw_threads;

    if (requested <= 0) {
        hw_threads = sixel_get_hw_threads();
        if (hw_threads < 1) {
            hw_threads = 1;
        }
        normalized = hw_threads;
    } else {
        normalized = requested;
    }
#else
    (void)requested;
    normalized = 1;
#endif
    if (normalized < 1) {
        normalized = 1;
    }
    return normalized;
}

/* Decoding stays serial unless the threading policy names a budget or a
 * decode limit; a decode limit never exceeds an explicit budget. */
static int
sixel_decoder_threads_resolve_policy(void)
{
    unsigned int decode_threads;
    int budget;
    int explicit_budget;

    decode_threads = 0u;
    budget = 1;
    explicit_budget = sixel_threading_policy_explicit_budget(&budget);
    if (sixel_threading_policy_stage_threads(SIXEL_THREADING_STAGE_DECODE,
                                             &decode_threads)) {
        if (decode_threads > (unsigned int)INT_MAX) {
            decode_threads = (unsigned int)INT_MAX;
        }
        if (explicit_budget && (int)decode_threads > budget) {
            return budget;
        }
        return sixel_decoder_threads_normalize((int)decode_threads);
    }
    if (explicit_budget) {
        return budget;
    }

    return 1;
}

/* Test hook: pin the decoder count independently of the threading policy. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_decoder_parallel_override_threads(char const *text)
{
    int budget;
    int serial;

    budget = 0;
    serial = 0;
    if (!sixel_option_parse_threading_policy_base(text, &budget, &serial)) {
        sixel_helper_set_additional_message(
            "decoder: thread count must be auto or a positive integer.");
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_decoder_threads_lock();
    g_decoder_threads.override_active = 1;
    g_decoder_threads.override_threads =
        sixel_decoder_threads_normalize(budget);
    sixel_decoder_threads_unlock();
    return SIXEL_OK;
}

SIXEL_INTERNAL_API int
sixel_decoder_parallel_resolve_threads(void)
{
    int threads;
    int override_active;

    threads = 1;
    sixel_decoder_threads_lock();
    override_active = g_decoder_threads.override_active;
    threads = g_decoder_threads.override_threads;
    sixel_decoder_threads_unlock();
    if (override_active) {
        threads = sixel_decoder_threads_normalize(threads);
    } else {
        threads = sixel_decoder_threads_resolve_policy();
    }
    if (threads < 1) {
        threads = 1;
    }
    return threads;
}

#if SIXEL_ENABLE_THREADS
static SIXELSTATUS
sixel_decoder_parallel_prepare_undither(
    sixel_decoder_undither_context_t *undither,
    image_buffer_t const *image,
    int const *source_palette,
    int palette_limit)
{
#if defined(SIXEL_DECODE_PIXELS_NO_FAST4)
    if (undither == NULL || !undither->enabled) {
        return SIXEL_OK;
    }
    (void)image;
    (void)source_palette;
    (void)palette_limit;
    /*
     * Embedded decoder cores may link this file without decoder.c.  Report
     * that fused fast4 is unavailable so the caller uses the serial path.
     */
    return SIXEL_FALSE;
#else
    unsigned char *dst;
    size_t pixels;
    size_t bytes;
    size_t pixel_index;
    int color;
    int n;
    int active_ncolors;
    int palette_size;

    if (undither == NULL || !undither->enabled) {
        return SIXEL_OK;
    }
    if (image == NULL || source_palette == NULL ||
            image->width <= 0 || image->height <= 0 ||
            undither->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (image->depth != 1U) {
        return SIXEL_FALSE;
    }
    if ((size_t)image->width > ((size_t)-1 / (size_t)image->height)) {
        return SIXEL_BAD_ALLOCATION;
    }

    active_ncolors = palette_limit;
    if (active_ncolors <= 0 ||
            active_ncolors > SIXEL_PALETTE_MAX_DECODER) {
        active_ncolors = image->ncolors;
    }
    if (active_ncolors <= 0) {
        active_ncolors = 1;
    } else if (active_ncolors > SIXEL_PALETTE_MAX_DECODER) {
        active_ncolors = SIXEL_PALETTE_MAX_DECODER;
    }
    /*
     * image_buffer_init() starts indexed decodes at two colors, but SIXEL
     * color selections may refer to the 256-color default palette without
     * redefining entries.  Keep every fused worker on the same practical
     * active palette floor so thread splits do not change similarity tests.
     */
    if (active_ncolors < 256) {
        active_ncolors = 256;
    }

    palette_size = SIXEL_PALETTE_MAX_DECODER;
    undither->ncolors = active_ncolors;
    undither->palette_size = palette_size;
    undither->pixel_size = undither->direct_output ? 4 : 3;

    if ((size_t)palette_size > ((size_t)-1 / 3u)) {
        return SIXEL_BAD_ALLOCATION;
    }
    undither->palette = (unsigned char *)calloc(
        (size_t)palette_size * 3u,
        1);
    if (undither->palette == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    for (n = 0; n < palette_size; ++n) {
        color = source_palette[n];
        undither->palette[n * 3 + 0] =
            (unsigned char)((color >> 16) & 0xff);
        undither->palette[n * 3 + 1] =
            (unsigned char)((color >> 8) & 0xff);
        undither->palette[n * 3 + 2] =
            (unsigned char)(color & 0xff);
    }

    pixels = (size_t)image->width * (size_t)image->height;
    if (pixels > ((size_t)-1 / (size_t)undither->pixel_size)) {
        free(undither->palette);
        undither->palette = NULL;
        return SIXEL_BAD_ALLOCATION;
    }
    bytes = pixels * (size_t)undither->pixel_size;
    undither->pixels = (unsigned char *)sixel_allocator_malloc(
        undither->allocator,
        bytes);
    if (undither->pixels == NULL) {
        free(undither->palette);
        undither->palette = NULL;
        return SIXEL_BAD_ALLOCATION;
    }

    dst = undither->pixels;
    for (pixel_index = 0u; pixel_index < pixels; ++pixel_index) {
        dst[0] = undither->palette[0];
        dst[1] = undither->palette[1];
        dst[2] = undither->palette[2];
        if (undither->direct_output) {
            dst[3] = 255u;
        }
        dst += undither->pixel_size;
    }

    return SIXEL_OK;
#endif
}
#endif

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decoder_parallel_request_start(int direct_mode,
                                     int ormode,
                                     unsigned char *input,
                                     int length,
                                     unsigned char *anchor,
                                     image_buffer_t *image,
                                     int initial_color_index,
                                     int const *palette,
                                     sixel_timeline_logger_t *logger,
                                     unsigned int decode_flags,
                                     int *painted_outside_raster,
                                     sixel_decoder_undither_context_t
                                         *undither)
{
#if SIXEL_ENABLE_THREADS
    SIXELSTATUS status;
    SIXELSTATUS prepare_status;
    int threads;
    int payload_start;
    int payload_len;
    sixel_thread_t *workers;
    sixel_decoder_worker_context_t *contexts;
    sixel_decoder_worker_chain_t chain;
    int global_capacity;
    int *spans;
    int i;
    int offset;
    int created;
    int parallel_failed;
    int runtime_error;
    int sync_ready;
    int pixel_size;
    int palette_limit;
    int max_color_index;
    int trust_raster_size;
    int undither_prepared;

    status = SIXEL_RUNTIME_ERROR;
    prepare_status = SIXEL_OK;
    workers = NULL;
    contexts = NULL;
    spans = NULL;
    threads = 0;
    payload_start = 0;
    payload_len = 0;
    global_capacity = 0;
    offset = 0;
    created = 0;
    parallel_failed = 0;
    runtime_error = 0;
    sync_ready = 0;
    pixel_size = 4;
    palette_limit = 0;
    max_color_index = (-1);
    undither_prepared = 0;
    trust_raster_size = (decode_flags &
        SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE) != 0U;
    if (painted_outside_raster != NULL) {
        *painted_outside_raster = 0;
    }
    memset(&chain, 0, sizeof(chain));

    if (input == NULL || anchor == NULL || length <= 0 || image == NULL) {
        runtime_error = 1;
        goto cleanup;
    }

    pixel_size = sixel_decoder_parallel_pixel_size(image->depth);
    if (ormode) {
        /* OR composition retains its serial plane parser and gather. These
         * workers store overwrite-mode indexes; using their column update
         * for OR paint would lose bitwise register composition. */
        status = SIXEL_FALSE;
        goto cleanup;
    }

    payload_start = (int)(anchor - input);
    if (payload_start < 0 || payload_start >= length) {
        runtime_error = 1;
        goto cleanup;
    }

    payload_len = length - payload_start;
    if (payload_len <= 0) {
        runtime_error = 1;
        goto cleanup;
    }

    palette_limit = image->ncolors;

    threads = sixel_decoder_parallel_resolve_threads();
    if (threads < 1) {
        threads = 1;
    }
    if (threads > payload_len) {
        threads = payload_len;
    }
    if (threads < 2) {
        status = SIXEL_FALSE;
        goto cleanup;
    }

    prepare_status = sixel_decoder_parallel_prepare_undither(
        undither,
        image,
        palette,
        palette_limit);
    if (prepare_status == SIXEL_FALSE) {
        status = SIXEL_FALSE;
        goto cleanup;
    }
    if (SIXEL_FAILED(prepare_status)) {
        runtime_error = 1;
        goto cleanup;
    }
    if (undither != NULL && undither->enabled && undither->pixels != NULL) {
        undither_prepared = 1;
    }

    workers = (sixel_thread_t *)calloc((size_t)threads,
                                       sizeof(sixel_thread_t));
    contexts = (sixel_decoder_worker_context_t *)calloc(
        (size_t)threads, sizeof(sixel_decoder_worker_context_t));
    spans = (int *)calloc((size_t)threads, sizeof(int));
    if (workers == NULL || contexts == NULL || spans == NULL) {
        runtime_error = 1;
        goto cleanup;
    }

    global_capacity = image->width * image->height;
    if (global_capacity <= 0) {
        runtime_error = 1;
        goto cleanup;
    }
    chain.global_buffer = (unsigned char *)image->pixels.p;
    if (chain.global_buffer == NULL) {
        runtime_error = 1;
        goto cleanup;
    }
    chain.global_mask = image->paint_mask;
    chain.pixel_size = pixel_size;
    chain.thread_count = threads;
    chain.global_capacity = global_capacity;
    sixel_mutex_init(&chain.mutex);
    sixel_cond_init(&chain.cond);
    sync_ready = 1;

    sixel_decoder_parallel_fill_spans(payload_len, threads, spans);

    if (logger != NULL) {
        /*
         * Record when the controller hands control to worker threads so the
         * timeline can show the gap between parser startup and worker
         * creation.
         */
        sixel_timeline_logger_logf(logger,
                          "decoder",
                          "controller",
                          "launch",
                          0,
                          0,
                          0,
                          length,
                          payload_start,
                          length,
                          "spawn %d workers payload=%d",
                          threads,
                          payload_len);
    }

    offset = payload_start;
    for (i = 0; i < threads; ++i) {
        contexts[i].chain = &chain;
        contexts[i].image = image;
        contexts[i].fail_columns =
            sixel_decoder_parallel_inject_failure("allocation", i);
        contexts[i].input = input;
        contexts[i].anchor = anchor;
        contexts[i].length = length;
        contexts[i].payload_len = payload_len;
        contexts[i].start_offset = offset;
        offset += spans[i];
        if (offset > length) {
            offset = length;
        }
        if (i == threads - 1) {
            contexts[i].end_offset = length - 1;
        } else {
            contexts[i].end_offset = offset - 1;
            if (contexts[i].end_offset < contexts[i].start_offset) {
                contexts[i].end_offset = contexts[i].start_offset;
            }
        }
        contexts[i].index = i;
        contexts[i].direct_mode = direct_mode;
        contexts[i].initial_color_index = initial_color_index;
        contexts[i].palette = palette;
        contexts[i].palette_limit = palette_limit;
        contexts[i].width = image->width;
        contexts[i].height = image->height;
        contexts[i].pixel_size = pixel_size;
        contexts[i].depth = image->depth;
        contexts[i].logger = logger;
        contexts[i].trust_raster_size = trust_raster_size;
        contexts[i].result = (-1);
        contexts[i].runtime_error = 0;
        contexts[i].first_painted_row = (-1);
        contexts[i].last_painted_row = (-1);
        contexts[i].direct_mode_kind = SIXEL_DECODER_DIRECT_PARSE;
        contexts[i].undither = undither_prepared ? undither : NULL;
    }

    created = 0;
    for (i = 0; i < threads; ++i) {
        contexts[i].result = (-1);
        contexts[i].runtime_error = 0;
        contexts[i].first_painted_row = (-1);
        contexts[i].last_painted_row = (-1);
        contexts[i].max_color_index = (-1);
        contexts[i].painted_outside_raster = 0;
        contexts[i].direct_mode_kind = SIXEL_DECODER_DIRECT_PARSE;
        if (sixel_decoder_parallel_inject_failure("parse", i)) {
            status = SIXEL_RUNTIME_ERROR;
        } else {
            status = sixel_thread_create(&workers[i],
                                         sixel_decoder_parallel_direct_worker,
                                         &contexts[i]);
        }
        if (SIXEL_FAILED(status)) {
            parallel_failed = 1;
            break;
        }
        created += 1;
    }

    for (i = 0; i < created; ++i) {
        sixel_thread_join(&workers[i]);
        if (contexts[i].result != 0) {
            parallel_failed = 1;
        }
        if (contexts[i].runtime_error) {
            runtime_error = 1;
        }
        if (contexts[i].painted_outside_raster &&
                painted_outside_raster != NULL) {
            *painted_outside_raster = 1;
        }
    }

    /* All ordinary columns are still private. Resolve and validate absolute
     * rows before releasing any gather; fast4 retains its halo windows. */
    if (!runtime_error && !parallel_failed && created == threads &&
            ((!undither_prepared &&
              !sixel_decoder_parallel_tail_compatible(contexts, threads)) ||
             !sixel_decoder_parallel_resolve_rows(contexts, threads))) {
        parallel_failed = 1;
    }

    /* Vertical clipping is known only after row offsets have been resolved. */
    for (i = 0; i < created; ++i) {
        if (contexts[i].painted_outside_raster &&
                painted_outside_raster != NULL) {
            *painted_outside_raster = 1;
        }
    }

    if (runtime_error || parallel_failed || created < threads) {
        status = runtime_error ? SIXEL_RUNTIME_ERROR : SIXEL_FALSE;
        goto cleanup;
    }

    if (!undither_prepared) {
        if (!runtime_error && !parallel_failed && created == threads) {
            max_color_index = (-1);
            created = 0;
            sixel_mutex_lock(&chain.mutex);
            chain.abort_requested = 0;
            chain.paint_ready = 0;
            chain.paint_released = 0;
            sixel_mutex_unlock(&chain.mutex);

            /*
             * Gather has no parser or allocation failure after release.
             * The aligned spans own distinct bands. Keep the release barrier
             * so even partial thread creation leaves the initialized shared
             * pixels and paint mask untouched for serial fallback.
             */
            for (i = 0; i < threads; ++i) {
                contexts[i].result = (-1);
                contexts[i].direct_mode_kind = SIXEL_DECODER_DIRECT_GATHER;
                if (sixel_decoder_parallel_inject_failure("gather", i)) {
                    status = SIXEL_RUNTIME_ERROR;
                } else {
                    status = sixel_thread_create(
                        &workers[i],
                        sixel_decoder_parallel_direct_worker,
                        &contexts[i]);
                }
                if (SIXEL_FAILED(status)) {
                    parallel_failed = 1;
                    sixel_decoder_parallel_cancel_decode(&chain);
                    break;
                }
                created += 1;
            }

            if (!parallel_failed && created == threads) {
                sixel_mutex_lock(&chain.mutex);
                while (chain.paint_ready < threads &&
                        !chain.abort_requested) {
                    sixel_cond_wait(&chain.cond, &chain.mutex);
                }
                if (!chain.abort_requested) {
                    chain.paint_released = 1;
                    sixel_cond_broadcast(&chain.cond);
                }
                sixel_mutex_unlock(&chain.mutex);
            }

            for (i = 0; i < created; ++i) {
                sixel_thread_join(&workers[i]);
                if (contexts[i].result != 0) {
                    parallel_failed = 1;
                    if (chain.paint_released) {
                        runtime_error = 1;
                    }
                }
                if (contexts[i].runtime_error) {
                    runtime_error = 1;
                }
                if (contexts[i].max_color_index > max_color_index) {
                    max_color_index = contexts[i].max_color_index;
                }
                if (contexts[i].painted_outside_raster &&
                        painted_outside_raster != NULL) {
                    *painted_outside_raster = 1;
                }
            }
        }

        if (runtime_error) {
            status = SIXEL_RUNTIME_ERROR;
        } else if (parallel_failed || created < threads) {
            status = SIXEL_FALSE;
        } else {
            status = SIXEL_OK;
        }
        goto cleanup;
    }

    created = 0;
    for (i = 0; i < threads; ++i) {
        status = sixel_thread_create(&workers[i],
                                     sixel_decoder_parallel_worker,
                                     &contexts[i]);
        if (SIXEL_FAILED(status)) {
            parallel_failed = 1;
            sixel_decoder_parallel_cancel_decode(&chain);
            break;
        }
        created += 1;
    }

    for (i = 0; i < created; ++i) {
        sixel_thread_join(&workers[i]);
        if (contexts[i].result != 0) {
            parallel_failed = 1;
        }
        if (contexts[i].max_color_index > max_color_index) {
            max_color_index = contexts[i].max_color_index;
        }
        if (contexts[i].painted_outside_raster &&
                painted_outside_raster != NULL) {
            *painted_outside_raster = 1;
        }
    }

    if (chain.abort_requested) {
        parallel_failed = 1;
    }

    if (runtime_error) {
        status = SIXEL_RUNTIME_ERROR;
    } else if (parallel_failed || created < threads) {
        status = SIXEL_FALSE;
    } else {
        status = SIXEL_OK;
    }

cleanup:
    if (sync_ready) {
        sixel_mutex_destroy(&chain.mutex);
        sixel_cond_destroy(&chain.cond);
    }
    if (undither != NULL) {
        free(undither->palette);
        undither->palette = NULL;
        if (status != SIXEL_OK && undither->pixels != NULL) {
            sixel_allocator_free(undither->allocator, undither->pixels);
            undither->pixels = NULL;
        }
    }

    if (contexts != NULL) {
        for (i = 0; i < threads; ++i) {
            free(contexts[i].columns);
        }
    }
    free(workers);
    free(contexts);
    free(spans);

    return status;
#else
    (void)direct_mode;
    (void)ormode;
    (void)input;
    (void)length;
    (void)anchor;
    (void)image;
    (void)initial_color_index;
    (void)palette;
    (void)logger;
    (void)decode_flags;
    (void)painted_outside_raster;
    (void)undither;

    return SIXEL_RUNTIME_ERROR;
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
