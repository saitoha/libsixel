/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Shared loader helpers used across backend implementations.  This module
 * centralizes trace logging, thumbnail size hints, and small detection
 * helpers so backend files stay narrow and platform headers remain isolated.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDARG_H
# include <stdarg.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif

#include <sixel.h>

#include "compat_stub.h"
#include "loader-common.h"
#include "logger.h"

static int loader_trace_enabled;
static int thumbnailer_default_size_hint = SIXEL_THUMBNAILER_DEFAULT_SIZE;
static int thumbnailer_size_hint = SIXEL_THUMBNAILER_DEFAULT_SIZE;
static int thumbnailer_size_hint_initialized;

void
loader_thumbnailer_initialize_size_hint(void)
{
    char const *env_value;
    char *endptr;
    long parsed;

    if (thumbnailer_size_hint_initialized) {
        return;
    }

    thumbnailer_size_hint_initialized = 1;
    thumbnailer_default_size_hint = SIXEL_THUMBNAILER_DEFAULT_SIZE;
    thumbnailer_size_hint = thumbnailer_default_size_hint;

    env_value = sixel_compat_getenv("SIXEL_THUMBNAILER_HINT_SIZE");
    if (env_value == NULL || env_value[0] == '\0') {
        return;
    }

    errno = 0;
    parsed = strtol(env_value, &endptr, 10);
    if (errno != 0) {
        return;
    }
    if (endptr == env_value || *endptr != '\0') {
        return;
    }
    if (parsed <= 0) {
        return;
    }
    if (parsed > (long)INT_MAX) {
        parsed = (long)INT_MAX;
    }

    thumbnailer_default_size_hint = (int)parsed;
    thumbnailer_size_hint = thumbnailer_default_size_hint;
}

int
loader_thumbnailer_get_size_hint(void)
{
    loader_thumbnailer_initialize_size_hint();

    return thumbnailer_size_hint;
}

int
loader_thumbnailer_get_default_size_hint(void)
{
    loader_thumbnailer_initialize_size_hint();

    return thumbnailer_default_size_hint;
}

void
sixel_helper_set_loader_trace(int enable)
{
    loader_trace_enabled = enable ? 1 : 0;
}

void
sixel_helper_set_thumbnail_size_hint(int size)
{
    loader_thumbnailer_initialize_size_hint();

    if (size > 0) {
        thumbnailer_size_hint = size;
    } else {
        thumbnailer_size_hint = thumbnailer_default_size_hint;
    }
}

void
loader_trace_message(char const *format, ...)
{
    va_list args;

    if (!loader_trace_enabled) {
        return;
    }

    fprintf(stderr, "libsixel: ");

    va_start(args, format);
    sixel_compat_vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

void
loader_trace_try(char const *name)
{
    if (loader_trace_enabled) {
        fprintf(stderr, "libsixel: trying %s loader\n", name);
    }
}

void
loader_trace_result(char const *name, SIXELSTATUS status)
{
    if (!loader_trace_enabled) {
        return;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "libsixel: loader %s succeeded\n", name);
    } else {
        fprintf(stderr, "libsixel: loader %s failed (%s)\n",
                name, sixel_helper_format_error(status));
    }
}

int
loader_trace_is_enabled(void)
{
    return loader_trace_enabled;
}

int
chunk_is_png(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 8) {
        return 0;
    }

    /*
     * PNG streams begin with an 8-byte signature.  Checking the fixed magic
     * sequence keeps the detection fast and avoids depending on libpng
     * helpers when only the signature is needed.
     */
    if (chunk->buffer[0] == (unsigned char)0x89 &&
        chunk->buffer[1] == 'P' &&
        chunk->buffer[2] == 'N' &&
        chunk->buffer[3] == 'G' &&
        chunk->buffer[4] == (unsigned char)0x0d &&
        chunk->buffer[5] == (unsigned char)0x0a &&
        chunk->buffer[6] == (unsigned char)0x1a &&
        chunk->buffer[7] == (unsigned char)0x0a) {
        return 1;
    }

    return 0;
}

int
chunk_is_jpeg(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 2) {
        return 0;
    }

    /*
     * JPEG files start with SOI (Start of Image) marker 0xFF 0xD8.  The GD
     * loader uses this to decide whether libgd should attempt JPEG decoding.
     */
    if (chunk->buffer[0] == (unsigned char)0xff &&
        chunk->buffer[1] == (unsigned char)0xd8) {
        return 1;
    }

    return 0;
}

int
chunk_is_bmp(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 2) {
        return 0;
    }

    /* BMP headers begin with the literal characters 'B' 'M'. */
    if (chunk->buffer[0] == 'B' && chunk->buffer[1] == 'M') {
        return 1;
    }

    return 0;
}

int
chunk_is_gif(sixel_chunk_t const *chunk)
{
    if (chunk->size < 6) {
        return 0;
    }
    if (chunk->buffer[0] == 'G' &&
        chunk->buffer[1] == 'I' &&
        chunk->buffer[2] == 'F' &&
        chunk->buffer[3] == '8' &&
        (chunk->buffer[4] == '7' || chunk->buffer[4] == '9') &&
        chunk->buffer[5] == 'a') {
        return 1;
    }
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
