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
# include "timeline-logger.h"
# include "threading.h"
#endif
#include "compat_stub.h"

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
    int ormode;
    int initial_color_index;
    int const *palette;
    int palette_limit;
    int width;
    int height;
    int pixel_size;
    int depth;
    sixel_timeline_logger_t *logger;
    int trust_raster_size;
    int painted_outside_raster;
    unsigned char *local_buffer;
    int local_capacity;
    int local_written;
    int max_color_index;
    int result;
    sixel_decoder_undither_context_t *undither;
    int body_local_start;
} sixel_decoder_worker_context_t;

typedef struct sixel_decoder_local_chunk {
    unsigned char *data;
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
} sixel_local_buffer_t;

typedef struct sixel_decoder_worker_chain {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    int *copy_offsets;
    int *copy_ready;
    int thread_count;
    unsigned char *global_buffer;
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
static pthread_once_t g_decoder_threads_once = PTHREAD_ONCE_INIT;

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

static int
sixel_decoder_parallel_skew_percent(void)
{
    char const *text;
    char *endptr;
    long value;

    /*
     * SIXEL_PARALLEL_SKEW lets operators bias span lengths by +/-20% so the
     * trailing workers take slightly more work.  The default keeps spans
     * balanced.
     */
    text = sixel_compat_getenv("SIXEL_PARALLEL_SKEW");
    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    errno = 0;
    value = strtol(text, &endptr, 10);
    if (errno != 0 || endptr == text || *endptr != '\0') {
        return 0;
    }

    if (value < -20) {
        value = -20;
    } else if (value > 20) {
        value = 20;
    }

    return (int)value;
}

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

static int
sixel_decoder_parallel_store_ormode_span(unsigned char *dst,
                                         int depth,
                                         int color_index,
                                         int repeat)
{
    unsigned char *bytes;
    unsigned short *shorts;
    int composed_index;
    int max_index;
    int n;

    if (dst == NULL || repeat <= 0) {
        return (-1);
    }

    max_index = (-1);
    if (depth == 1) {
        bytes = dst;
        for (n = 0; n < repeat; ++n) {
            composed_index = bytes[n] | color_index;
            bytes[n] = (unsigned char)composed_index;
            if (max_index < composed_index) {
                max_index = composed_index;
            }
        }
    } else {
        shorts = (unsigned short *)dst;
        for (n = 0; n < repeat; ++n) {
            composed_index = shorts[n] | color_index;
            shorts[n] = (unsigned short)composed_index;
            if (max_index < composed_index) {
                max_index = composed_index;
            }
        }
    }

    return max_index;
}

static void
sixel_local_buffer_init(sixel_local_buffer_t *buffer,
                        int width,
                        int pixel_size)
{
    buffer->head = NULL;
    buffer->tail = NULL;
    buffer->cursor = NULL;
    buffer->width = width;
    buffer->pixel_size = pixel_size;
}

static void
sixel_local_buffer_dispose(sixel_local_buffer_t *buffer)
{
    sixel_decoder_local_chunk_t *cursor;

    cursor = buffer->head;
    while (cursor != NULL) {
        sixel_decoder_local_chunk_t *tmp;

        free(cursor->data);
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
sixel_decoder_parallel_count_newlines(unsigned char const *begin,
                                      unsigned char const *end)
{
    unsigned char const *scan;
    int lines;

    lines = 0;
    scan = begin;
    while (scan != NULL && scan < end) {
        if (*scan == '-') {
            lines += 1;
        }
        ++scan;
    }

    return lines;
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
 * Worker entry for the fast sixel parser.  Each thread jumps to the next
 * '-' marker from its assigned offset, then walks tokens until the first
 * '-' after its scheduled end offset.
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
    unsigned char *anchor = NULL;
    unsigned char *scan = NULL;
    unsigned char *cursor = NULL;
    unsigned char *body_cursor = NULL;
    unsigned char *decode_cursor = NULL;
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
    int line_count = 0;
    int copy_offset = 0;
    int copy_span = 0;
    int next_offset = 0;
    int i;
    int fallback = 0;
    int status = (-1);
    sixel_timeline_logger_t *logger = NULL;
    int width = 0;
    int pixel_size = 0;
    int depth = 0;
    int height = 0;
    int chain_offset = 0;
    int starts_after_newline = 0;
    int max_color_index = (-1);
    int span_max_index = (-1);
    int effective_repeat = 0;
    int local_rows = 0;
    unsigned char ch;

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
    anchor = context->anchor;
    if (context->input == NULL || context->length <= 0) {
        context->result = status;
        return status;
    }

    width = context->width;
    if (width <= 0) {
        context->result = status;
        return status;
    }

    pixel_size = context->pixel_size;
    depth = context->depth;
    height = context->height;
    if (height <= 0) {
        context->result = status;
        return status;
    }
    sixel_local_buffer_init(&local_buffer, width, pixel_size);
    if (context->payload_len <= 0) {
        context->result = status;
        return status;
    }

    start = context->input + context->start_offset;
    if (start < context->input ||
            start >= context->input + context->length) {
        context->result = status;
        return status;
    }

    assigned = context->end_offset - context->start_offset + 1;
    if (assigned <= 0) {
        context->result = status;
        return status;
    }

    /*
     * The controller starts workers after palette/raster attributes are
     * established.  Carry the current palette index across that anchor;
     * otherwise chunks that begin with raster data would silently use color 0.
     */
    color_index = context->initial_color_index;
    if (color_index < 0) {
        color_index = 0;
    } else if (color_index >= SIXEL_PALETTE_MAX_DECODER) {
        color_index = SIXEL_PALETTE_MAX_DECODER - 1;
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

    cursor = start;
    if (context->index > 0) {
        /*
         * A byte span can start immediately after DECGNL ('-') when the
         * previous worker's inclusive end offset landed on that delimiter.
         * In that case this worker is already positioned at the next sixel
         * band. Searching for another '-' would skip exactly one 6-pixel band.
         */
        starts_after_newline = start > anchor && start[-1] == '-';
        if (!starts_after_newline) {
            cursor = (unsigned char *)memchr(start,
                                             '-',
                                             (size_t)(context->length -
                                             context->start_offset));
            if (cursor != NULL &&
                    cursor + 1 < context->input + context->length) {
                cursor += 1;
            } else {
                cursor = start;
            }
        }
    }
    if (context->index > 0 && cursor == start && !starts_after_newline) {
        status = (-1);
        context->result = status;
        return status;
    }

    body_cursor = cursor;
    decode_cursor = body_cursor;
    context->body_local_start = 0;
    if (context->undither != NULL && context->undither->enabled) {
        decode_cursor = sixel_decoder_parallel_find_fast4_halo(
            anchor,
            body_cursor);
        context->body_local_start =
            sixel_decoder_parallel_count_newlines(
                decode_cursor,
                body_cursor) * 6;
        cursor = decode_cursor;
    }

    if (anchor != NULL && anchor < cursor) {
        scan = anchor;
        while (scan < cursor) {
            if (*scan == '-') {
                line_count += 1;
                scan += 1;
                continue;
            }

            if (*scan == '#') {
                int value;
                unsigned char *p;

                value = 0;
                p = scan + 1;
                while (p < cursor && *p >= '0' && *p <= '9') {
                    value = value * 10 + (*p - '0');
                    p += 1;
                }
                color_index = value;
                if (color_index < 0) {
                    color_index = 0;
                }
                if (color_index >= SIXEL_PALETTE_MAX_DECODER) {
                    color_index = SIXEL_PALETTE_MAX_DECODER - 1;
                }
                if (p < cursor && *p == ';') {
                    scan = p;
                    continue;
                }
                scan = p;
                continue;
            }

            scan += 1;
        }
    }
    row_offset = line_count * 6;

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
                    if (row_offset + pos_y + i >= height) {
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

                    if (context->ormode) {
                        span_max_index =
                            sixel_decoder_parallel_store_ormode_span(
                                chunk_cursor->data +
                                (size_t)row_base * (size_t)pixel_size,
                                depth,
                                color_index,
                                effective_repeat);
                        if (span_max_index > max_color_index) {
                            max_color_index = span_max_index;
                        }
                    } else if (pixel_size == 1 && effective_repeat > 3) {
                        memset(chunk_cursor->data + (size_t)row_base,
                               color_index,
                               effective_repeat);
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
            int value;
            unsigned char *p;

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
            int value;
            unsigned char *p;

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

    copy_span = max_relative + 1;
    if (copy_span < 0) {
        copy_span = 0;
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
        sixel_mutex_lock(&chain->mutex);
        chain->abort_requested = 1;
        sixel_cond_broadcast(&chain->cond);
        sixel_mutex_unlock(&chain->mutex);
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
            sixel_mutex_lock(&chain->mutex);
            chain->abort_requested = 1;
            sixel_cond_broadcast(&chain->cond);
            sixel_mutex_unlock(&chain->mutex);
            sixel_local_buffer_dispose(&local_buffer);
            context->result = status;
            return status;
        }
        sixel_local_buffer_dispose(&local_buffer);
        context->local_buffer = NULL;
        context->local_capacity = 0;
        context->local_written = 0;
        context->max_color_index = max_color_index;
        context->result = status;
        return status;
    }
#endif

    context->local_buffer = local_buffer.head != NULL ?
        local_buffer.head->data : NULL;
    context->local_capacity = capacity;
    context->local_written = copy_span;

    sixel_mutex_lock(&chain->mutex);
    while (!chain->copy_ready[context->index] && !chain->abort_requested) {
        sixel_cond_wait(&chain->cond, &chain->mutex);
    }
    if (chain->abort_requested) {
        sixel_mutex_unlock(&chain->mutex);
        sixel_local_buffer_dispose(&local_buffer);
        context->local_buffer = NULL;
        context->local_capacity = 0;
        context->local_written = 0;
        status = (-1);
        context->result = status;
        return status;
    }
    chain_offset = chain->copy_offsets[context->index];
    copy_offset = chain_offset;
    next_offset = copy_offset + copy_span;
    context->local_written = copy_span;
    if (context->index + 1 < chain->thread_count) {
        chain->copy_offsets[context->index + 1] = next_offset;
        chain->copy_ready[context->index + 1] = 1;
    }
    sixel_cond_broadcast(&chain->cond);
    sixel_mutex_unlock(&chain->mutex);

    if (copy_offset < 0 || chain->global_buffer == NULL) {
        sixel_mutex_lock(&chain->mutex);
        chain->abort_requested = 1;
        sixel_cond_broadcast(&chain->cond);
        sixel_mutex_unlock(&chain->mutex);
        sixel_local_buffer_dispose(&local_buffer);
        context->local_buffer = NULL;
        context->local_capacity = 0;
        context->local_written = 0;
        status = (-1);
        context->result = status;
        return status;
    }

    if (logger != NULL) {
        /* Chain memcpy execution so the timeline shows the serialized copy */
        sixel_timeline_logger_logf(logger,
                          "copy",
                          "decoder",
                          "start",
                          context->index,
                          context->index,
                          0,
                          0,
                          copy_offset,
                          next_offset,
                          "worker %d memcpy count=%d",
                          context->index,
                          copy_span);
    }

    if (max_relative >= 0) {
        chunk_cursor = local_buffer.head;
        while (chunk_cursor != NULL) {
            int rows;
            size_t chunk_bytes;
            size_t chunk_offset;

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
                          copy_offset,
                          next_offset,
                          "worker %d memcpy done", 
                          context->index);
    }

    sixel_local_buffer_dispose(&local_buffer);
    context->local_buffer = NULL;
    context->local_capacity = 0;
    context->local_written = 0;

    context->max_color_index = max_color_index;
    context->result = status;
    return status;
}
#endif

static int
sixel_decoder_threads_token_is_auto(char const *text)
{
    if (text == NULL) {
        return 0;
    }
    if ((text[0] == 'a' || text[0] == 'A') &&
            (text[1] == 'u' || text[1] == 'U') &&
            (text[2] == 't' || text[2] == 'T') &&
            (text[3] == 'o' || text[3] == 'O') &&
            text[4] == '\0') {
        return 1;
    }
    return 0;
}

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

static int
sixel_decoder_threads_parse_value(char const *text, int *value)
{
    long parsed;
    char *endptr;
    int normalized;

    if (text == NULL || value == NULL) {
        return 0;
    }
    if (sixel_decoder_threads_token_is_auto(text)) {
        normalized = sixel_decoder_threads_normalize(0);
        *value = normalized;
        return 1;
    }
    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0' || errno == ERANGE) {
        return 0;
    }
    if (parsed < 1) {
        normalized = sixel_decoder_threads_normalize(1);
    } else if (parsed > INT_MAX) {
        normalized = sixel_decoder_threads_normalize(INT_MAX);
    } else {
        normalized = sixel_decoder_threads_normalize((int)parsed);
    }
    *value = normalized;
    return 1;
}

static int
sixel_decoder_threads_resolve_env(void)
{
    char const *text;
    int parsed;

    text = sixel_compat_getenv("SIXEL_THREADS");
    if (text == NULL || text[0] == '\0') {
        return 1;
    }
    if (sixel_decoder_threads_parse_value(text, &parsed)) {
        return sixel_decoder_threads_normalize(parsed);
    }

    return 1;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decoder_parallel_override_threads(char const *text)
{
    SIXELSTATUS status;
    int parsed;

    status = SIXEL_BAD_ARGUMENT;
    if (text == NULL || text[0] == '\0') {
        sixel_helper_set_additional_message(
            "decoder: missing thread count after -=/--threads.");
        goto end;
    }
    if (!sixel_decoder_threads_parse_value(text, &parsed)) {
        sixel_helper_set_additional_message(
            "decoder: threads must be a positive integer or 'auto'.");
        goto end;
    }
    sixel_decoder_threads_lock();
    g_decoder_threads.override_active = 1;
    g_decoder_threads.override_threads = parsed;
    sixel_decoder_threads_unlock();
    status = SIXEL_OK;
end:
    return status;
}

SIXEL_INTERNAL_API int
sixel_decoder_parallel_resolve_threads(void)
{
    int threads;

    threads = 1;
    sixel_decoder_threads_lock();
    if (g_decoder_threads.override_active) {
        threads = sixel_decoder_threads_normalize(
            g_decoder_threads.override_threads);
    } else {
        threads = sixel_decoder_threads_resolve_env();
    }
    sixel_decoder_threads_unlock();
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
    int *copy_offsets;
    int *copy_ready;
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
    copy_offsets = NULL;
    copy_ready = NULL;
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
    if (ormode && image->depth == 4U) {
        if (image->ormode_indexes == NULL) {
            runtime_error = 1;
            goto cleanup;
        }
        pixel_size = (int)sizeof(*image->ormode_indexes);
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
    copy_offsets = (int *)calloc((size_t)threads, sizeof(int));
    copy_ready = (int *)calloc((size_t)threads, sizeof(int));
    if (workers == NULL || contexts == NULL || spans == NULL ||
            copy_offsets == NULL || copy_ready == NULL) {
        runtime_error = 1;
        goto cleanup;
    }

    global_capacity = image->width * image->height;
    if (global_capacity <= 0) {
        runtime_error = 1;
        goto cleanup;
    }
    if (ormode && image->depth == 4U) {
        chain.global_buffer = (unsigned char *)image->ormode_indexes;
    } else {
        chain.global_buffer = (unsigned char *)image->pixels.p;
    }
    chain.pixel_size = pixel_size;
    chain.copy_offsets = copy_offsets;
    chain.copy_ready = copy_ready;
    chain.thread_count = threads;
    chain.global_capacity = global_capacity;
    sixel_mutex_init(&chain.mutex);
    sixel_cond_init(&chain.cond);
    chain.copy_ready[0] = 1;
    chain.copy_offsets[0] = 0;
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
    created = 0;
    for (i = 0; i < threads; ++i) {
        contexts[i].chain = &chain;
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
        contexts[i].ormode = ormode;
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
        contexts[i].undither = undither_prepared ? undither : NULL;
        status = sixel_thread_create(&workers[i],
                                     sixel_decoder_parallel_worker,
                                     &contexts[i]);
        if (SIXEL_FAILED(status)) {
            runtime_error = 1;
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

    if (runtime_error || created < threads) {
        status = SIXEL_RUNTIME_ERROR;
    } else if (parallel_failed) {
        status = SIXEL_FALSE;
    } else {
        if (ormode && max_color_index >= 0 &&
                max_color_index + 1 > image->ncolors) {
            image->ncolors = max_color_index + 1;
        }
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

    free(workers);
    free(contexts);
    free(spans);
    free(copy_offsets);
    free(copy_ready);

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
