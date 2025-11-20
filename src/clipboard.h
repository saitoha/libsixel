/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
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

/*
 * Clipboard helpers shared between the core library and the converter
 * front-ends.  The routines expose three pillars:
 *
 *   1. Specification parsing so callers can recognise the
 *      "[format:]clipboard:" pseudo target.
 *   2. Byte oriented read/write APIs used to exchange data with the
 *      platform clipboard.
 *   3. Availability probes that allow tests to skip gracefully when the
 *      desktop session refuses clipboard access.
 */

#ifndef LIBSIXEL_CLIPBOARD_H
#define LIBSIXEL_CLIPBOARD_H

#include "config.h"

#include <stddef.h>

#include <sixel.h>

typedef struct sixel_clipboard_spec {
    int is_clipboard;
    char format[32];
} sixel_clipboard_spec_t;

int sixel_clipboard_parse_spec(char const *spec,
                               sixel_clipboard_spec_t *out);

SIXELSTATUS sixel_clipboard_read(char const *format,
                                 unsigned char **data,
                                 size_t *size,
                                 sixel_allocator_t *allocator);

SIXELSTATUS sixel_clipboard_write(char const *format,
                                  unsigned char const *data,
                                  size_t size);

int sixel_clipboard_is_available(void);

#endif

