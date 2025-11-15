/*
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_OPTIONS_H
#define LIBSIXEL_OPTIONS_H

#include <stddef.h>

/*
 * The choice descriptor couples the textual prefix with the integral
 * payload stored by the caller.  The helper functions only inspect the
 * `name` field when matching, returning the associated `value` on
 * success.
 */
typedef struct sixel_option_choice {
    char const *name;
    int value;
} sixel_option_choice_t;

typedef enum sixel_option_choice_result {
    SIXEL_OPTION_CHOICE_MATCH = 0,
    SIXEL_OPTION_CHOICE_AMBIGUOUS = 1,
    SIXEL_OPTION_CHOICE_NONE = 2
} sixel_option_choice_result_t;

/*
 * The filesystem validator accepts caller-defined flags controlling special
 * pseudo paths.  Remote URLs, clipboard pseudo paths, and standard input may
 * bypass the on-disk existence check when the corresponding bit is present.
 */
#define SIXEL_OPTION_PATH_ALLOW_STDIN       (1u << 0)
#define SIXEL_OPTION_PATH_ALLOW_CLIPBOARD   (1u << 1)
#define SIXEL_OPTION_PATH_ALLOW_REMOTE      (1u << 2)
#define SIXEL_OPTION_PATH_ALLOW_EMPTY       (1u << 3)

int
sixel_option_validate_filesystem_path(
    char const *argument,
    char const *resolved_path,
    unsigned int flags);

sixel_option_choice_result_t
sixel_option_match_choice(
    char const *value,
    sixel_option_choice_t const *choices,
    size_t choice_count,
    int *matched_value,
    char *diagnostic,
    size_t diagnostic_size);

void
sixel_option_report_ambiguous_prefix(
    char const *value,
    char const *candidates,
    char *buffer,
    size_t buffer_size);

#endif /* !defined(LIBSIXEL_OPTIONS_H) */
