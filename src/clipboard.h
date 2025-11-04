/*
 * SPDX-License-Identifier: MIT
 *
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

