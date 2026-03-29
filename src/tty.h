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

#ifndef LIBSIXEL_TTY_H
#define LIBSIXEL_TTY_H

#include <stddef.h>

struct sixel_tty_output_state {
    int is_tty;
    int use_ansi_sequences;
    int supports_bold;
    int supports_color;
};

SIXEL_INTERNAL_API struct sixel_tty_output_state const *
sixel_tty_get_output_state(void);

SIXELSTATUS
sixel_tty_wait_stdin(int usec);

SIXELSTATUS
sixel_tty_scroll(
    sixel_write_function f_write,
    void *priv,
    int outfd,
    int height,
    int is_animation);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_parse_colorspec(unsigned char *bgcolor, char const *text);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_parse_osc11_response(unsigned char *bgcolor,
                               char const *response,
                               size_t response_size);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_query_osc11_bgcolor(unsigned char *bgcolor, int timeout_ms);

typedef int (*sixel_tty_query_stop_function)(void *context);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_query_osc11_bgcolor_with_drain(
    unsigned char *bgcolor,
    int timeout_ms,
    sixel_tty_query_stop_function should_stop,
    void *context);

SIXEL_INTERNAL_API int
sixel_tty_is_animation_hide_cursor_enabled(char const *value);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_hide_cursor(int fd);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_restore_cursor(int fd);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_begin_animation_input_guard(void);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_end_animation_input_guard(void);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_restore_animation_cursor_to_bottom(int outfd, int height);

SIXEL_INTERNAL_API void
sixel_tty_restore_cbreak_for_abort(void);

#endif /* LIBSIXEL_TTY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
