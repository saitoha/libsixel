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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */

#if HAVE_TIME_H
# include <time.h>
#elif HAVE_SYS_TIME_H
# include <sys/time.h>
#endif  /* HAVE_SYS_TIME_H */
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#if HAVE_UNISTD_H
# include <unistd.h>
#elif HAVE_SYS_UNISTD_H
# include <sys/unistd.h>
#endif  /* HAVE_SYS_UNISTD_H */
#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif  /* HAVE_SYS_SELECT_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_TERMIOS_H
# include <termios.h>
#endif  /* HAVE_TERMIOS_H */
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif  /* HAVE_SYS_IOCTL_H */
#if HAVE_SYS_TTYCOM_H
/* Some systems expose winsize/TIOCGWINSZ in sys/ttycom.h. */
# include <sys/ttycom.h>
#endif  /* HAVE_SYS_TTYCOM_H */
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif  /* HAVE_FCNTL_H */

#include <sixel.h>
#include "tty.h"
#include "compat_stub.h"
#include "loader-common.h"
#include "rgblookup.h"
#include "timer.h"

#if defined(_WIN32)
# if !defined(UNICODE)
#  define UNICODE
# endif
# if !defined(_UNICODE)
#  define _UNICODE
# endif
# if !defined(WIN32_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# include <io.h>
# if !defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
#  define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
# endif
#endif

/* for msvc */
#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif

/*
 * Cache describing the capabilities of the active output device.
 * The struct lives in static storage because the helper routines are
 * frequently used from the CLI tools without an explicit lifecycle.
 */
static struct sixel_tty_output_state tty_output_state = {0, 0, 0, 0};

static char const g_tty_hide_cursor_seq[] = "\033[?25l";
static char const g_tty_show_cursor_seq[] = "\033[?25h";

/*
 * OpenVMS/GNV can provide <termios.h> without tcgetattr()/tcsetattr() linker
 * symbols. Keep all cbreak-mode code behind the function probes as well as
 * the header probes so the final native LINK step has no unresolved symbols.
 */
#if HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY \
    && HAVE_TCGETATTR && HAVE_TCSETATTR
# define SIXEL_TTY_HAVE_TERMIOS_CBREAK 1
#else
# define SIXEL_TTY_HAVE_TERMIOS_CBREAK 0
#endif

/*
 * Cursor position queries are only used by the pixel-height scroll path.
 * Keep the helper and the call site behind the same capability macro so
 * warning-clean builds do not see an unused static fallback on targets that
 * cannot enter that path.
 */
#if SIXEL_TTY_HAVE_TERMIOS_CBREAK \
    && !defined(__EMSCRIPTEN__) && defined(TIOCGWINSZ)
# define SIXEL_TTY_HAVE_CURSOR_POSITION_QUERY 1
#else
# define SIXEL_TTY_HAVE_CURSOR_POSITION_QUERY 0
#endif

static int
sixel_tty_term_supports_ansi(const char *term);

static int
sixel_tty_term_supports_color(const char *term, const char *colorterm);

static char *
sixel_tty_strdup(char const *text);

static SIXELSTATUS
sixel_tty_wait_fd_readable(int fd, int usec, int *is_readable);

static int
sixel_tty_parse_cpr_positive_int(char const *response,
                                 size_t response_size,
                                 size_t *cursor,
                                 int *value);

#if SIXEL_TTY_HAVE_CURSOR_POSITION_QUERY
static SIXELSTATUS
sixel_tty_query_cursor_position(int fd, int timeout_ms, int *row, int *col);
#endif

#if SIXEL_TTY_HAVE_TERMIOS_CBREAK
#define SIXEL_TTY_ABORT_RESTORE_MAX 8

typedef struct sixel_tty_abort_restore_slot {
    volatile int active;
    int fd;
    int has_saved_termios;
    volatile int cursor_hidden;
    struct termios old_termios;
} sixel_tty_abort_restore_slot_t;

static sixel_tty_abort_restore_slot_t
    g_tty_abort_restore_slots[SIXEL_TTY_ABORT_RESTORE_MAX];

static SIXELSTATUS
sixel_tty_register_cbreak_fd(int fd, struct termios const *old_termios);

static void
sixel_tty_unregister_cbreak_fd(int fd);

static int
sixel_tty_find_abort_restore_slot(int fd);

static int
sixel_tty_reserve_abort_restore_slot(int fd);

static SIXELSTATUS
sixel_tty_cbreak_fd(int fd,
                    struct termios *old_termios,
                    struct termios *new_termios);

static SIXELSTATUS
sixel_tty_restore_fd(int fd, struct termios *old_termios);
#endif  /* SIXEL_TTY_HAVE_TERMIOS_CBREAK */

static int
sixel_tty_term_supports_ansi(const char *term)
{
    size_t i;
    size_t count;
    const char *const *entry;
    static const char *const denylist[] = {
        "dumb",
        "emacs",
        "unknown",
        "cons25",
        "vt100-nam"
    };
    static const char *const allowlist[] = {
        "ansi",
        "color",
        "xterm",
        "rxvt",
        "tmux",
        "screen",
        "linux",
        "foot",
        "wezterm",
        "alacritty",
        "konsole",
        "kitty",
        "gnome",
        "eterm",
        "cygwin",
        "putty",
        "vt100",
        "vt102",
        "vt220",
        "st-",
        "st"
    };

    if (term == NULL) {
        return 0;
    }

    count = sizeof(denylist) / sizeof(denylist[0]);
    for (i = 0; i < count; ++i) {
        entry = &denylist[i];
        if (strcmp(term, *entry) == 0) {
            return 0;
        }
    }

    count = sizeof(allowlist) / sizeof(allowlist[0]);
    for (i = 0; i < count; ++i) {
        entry = &allowlist[i];
        if (strstr(term, *entry) != NULL) {
            return 1;
        }
    }

    return 0;
}

static int
sixel_tty_term_supports_color(const char *term, const char *colorterm)
{
    size_t i;
    size_t count;
    const char *const *entry;
    static const char *const allowlist[] = {
        "256color",
        "color",
        "xterm",
        "rxvt",
        "tmux",
        "screen",
        "linux",
        "foot",
        "wezterm",
        "alacritty",
        "konsole",
        "kitty",
        "gnome",
        "eterm",
        "cygwin",
        "putty",
        "vt220",
        "vt340",
        "ansi"
    };

    if (colorterm != NULL && colorterm[0] != '\0') {
        return 1;
    }

    if (term == NULL) {
        return 0;
    }

    if (strstr(term, "mono") != NULL || strstr(term, "bw") != NULL) {
        return 0;
    }

    count = sizeof(allowlist) / sizeof(allowlist[0]);
    for (i = 0; i < count; ++i) {
        entry = &allowlist[i];
        if (strstr(term, *entry) != NULL) {
            return 1;
        }
    }

    return 0;
}

static char *
sixel_tty_strdup(char const *text)
{
    char *copy;
    size_t length;

    copy = NULL;
    length = 0u;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1u);
    return copy;
}

SIXELSTATUS
sixel_tty_parse_colorspec(unsigned char *bgcolor, char const *text)
{
    SIXELSTATUS status;
    char *p;
    unsigned char components[3];
    int component_index;
    unsigned long value;
    char *endptr;
    char *buf;
    struct color const *named_color;
    size_t name_length;

    status = SIXEL_BAD_ARGUMENT;
    p = NULL;
    components[0] = 0u;
    components[1] = 0u;
    components[2] = 0u;
    component_index = 0;
    value = 0u;
    endptr = NULL;
    buf = NULL;
    named_color = NULL;
    name_length = 0u;

    if (bgcolor == NULL || text == NULL || text[0] == '\0') {
        return SIXEL_BAD_ARGUMENT;
    }

    name_length = strlen(text);
    if (name_length > (size_t)UINT_MAX) {
        return SIXEL_BAD_ARGUMENT;
    }

    named_color = lookup_rgb(text, (unsigned int)name_length);
    if (named_color != NULL) {
        bgcolor[0] = named_color->r;
        bgcolor[1] = named_color->g;
        bgcolor[2] = named_color->b;
        return SIXEL_OK;
    }

    if (text[0] == 'r' && text[1] == 'g' && text[2] == 'b'
            && text[3] == ':') {
        p = buf = sixel_tty_strdup(text + 4);
        if (buf == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        while (*p != '\0') {
            value = 0u;
            for (endptr = p; endptr - p <= 12; ++endptr) {
                if (*endptr >= '0' && *endptr <= '9') {
                    value = (value << 4) | (unsigned long)(*endptr - '0');
                } else if (*endptr >= 'a' && *endptr <= 'f') {
                    value = (value << 4)
                        | (unsigned long)(*endptr - 'a' + 10);
                } else if (*endptr >= 'A' && *endptr <= 'F') {
                    value = (value << 4)
                        | (unsigned long)(*endptr - 'A' + 10);
                } else {
                    break;
                }
            }
            if (endptr - p == 0 || endptr - p > 4) {
                break;
            }
            value = value << ((4 - (endptr - p)) * 4) >> 8;
            components[component_index++] = (unsigned char)value;
            p = endptr;
            if (component_index == 3) {
                break;
            }
            if (*p == '\0' || *p != '/') {
                break;
            }
            ++p;
        }
        if (component_index != 3 || *p != '\0' || *p == '/') {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        bgcolor[0] = components[0];
        bgcolor[1] = components[1];
        bgcolor[2] = components[2];
        status = SIXEL_OK;
        goto end;
    }

    if (*text == '#') {
        buf = sixel_tty_strdup(text + 1);
        if (buf == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        for (p = endptr = buf; endptr - p <= 12; ++endptr) {
            if (*endptr >= '0' && *endptr <= '9') {
                *endptr = (char)(*endptr - '0');
            } else if (*endptr >= 'a' && *endptr <= 'f') {
                *endptr = (char)(*endptr - 'a' + 10);
            } else if (*endptr >= 'A' && *endptr <= 'F') {
                *endptr = (char)(*endptr - 'A' + 10);
            } else if (*endptr == '\0') {
                break;
            } else {
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        if (endptr - p > 12) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        switch (endptr - p) {
        case 3:
            bgcolor[0] = (unsigned char)(p[0] << 4);
            bgcolor[1] = (unsigned char)(p[1] << 4);
            bgcolor[2] = (unsigned char)(p[2] << 4);
            break;
        case 6:
            bgcolor[0] = (unsigned char)(p[0] << 4 | p[1]);
            bgcolor[1] = (unsigned char)(p[2] << 4 | p[3]);
            bgcolor[2] = (unsigned char)(p[4] << 4 | p[4]);
            break;
        case 9:
            bgcolor[0] = (unsigned char)(p[0] << 4 | p[1]);
            bgcolor[1] = (unsigned char)(p[3] << 4 | p[4]);
            bgcolor[2] = (unsigned char)(p[6] << 4 | p[7]);
            break;
        case 12:
            bgcolor[0] = (unsigned char)(p[0] << 4 | p[1]);
            bgcolor[1] = (unsigned char)(p[4] << 4 | p[5]);
            bgcolor[2] = (unsigned char)(p[8] << 4 | p[9]);
            break;
        default:
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        status = SIXEL_OK;
        goto end;
    }

    status = SIXEL_BAD_ARGUMENT;

end:
    if (buf != NULL) {
        free(buf);
    }

    return status;
}

SIXELSTATUS
sixel_tty_parse_osc11_response(unsigned char *bgcolor,
                               char const *response,
                               size_t response_size)
{
    size_t index;
    size_t start;
    size_t end;
    char *colorspec;
    SIXELSTATUS status;

    index = 0u;
    start = 0u;
    end = 0u;
    colorspec = NULL;
    status = SIXEL_BAD_ARGUMENT;

    if (bgcolor == NULL || response == NULL || response_size == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (index = 0u; index + 5u <= response_size; ++index) {
        if ((unsigned char)response[index + 0u] != 0x1bu
            || response[index + 1u] != ']'
            || response[index + 2u] != '1'
            || response[index + 3u] != '1'
            || response[index + 4u] != ';') {
            continue;
        }
        start = index + 5u;
        for (end = start; end < response_size; ++end) {
            if ((unsigned char)response[end] == 0x07u) {
                break;
            }
            if ((unsigned char)response[end] == 0x1bu
                    && end + 1u < response_size
                    && response[end + 1u] == '\\') {
                break;
            }
        }
        if (end >= response_size || end <= start) {
            continue;
        }
        colorspec = (char *)malloc(end - start + 1u);
        if (colorspec == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        memcpy(colorspec, response + start, end - start);
        colorspec[end - start] = '\0';
        status = sixel_tty_parse_colorspec(bgcolor, colorspec);
        free(colorspec);
        return status;
    }

    return status;
}

SIXELAPI void
sixel_tty_init_output_device(int fd)
{
    int istty;
    const char *term;
    const char *colorterm;
    struct sixel_tty_output_state *state;
#if defined(_WIN32)
    intptr_t handle_value;
    HANDLE handle;
    DWORD mode;
    DWORD desired;
#endif

    state = &tty_output_state;
    state->is_tty = 0;
    state->use_ansi_sequences = 0;
    state->supports_bold = 0;
    state->supports_color = 0;
    istty = 0;

#if !HAVE_ISATTY && !defined(_WIN32)
    (void)fd;
#endif

#if HAVE_ISATTY
    if (sixel_compat_isatty(fd)) {
        istty = 1;
    }
#endif

    if (istty == 0) {
        return;
    }

    state->is_tty = 1;

#if defined(_WIN32)
    handle_value = _get_osfhandle(fd);
    if (handle_value != (intptr_t)-1) {
        handle = (HANDLE)handle_value;
        if (GetConsoleMode(handle, &mode) != 0) {
            desired = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            if (SetConsoleMode(handle, desired) != 0) {
                mode = desired;
            }
            if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
                state->use_ansi_sequences = 1;
                state->supports_bold = 1;
                state->supports_color = 1;
                return;
            }
        }
    }
#endif

    term = sixel_compat_getenv("TERM");
    colorterm = sixel_compat_getenv("COLORTERM");

    if (term == NULL || term[0] == '\0') {
        return;
    }

    if (sixel_tty_term_supports_ansi(term) != 0) {
        state->use_ansi_sequences = 1;
        state->supports_bold = 1;
    }

    if (state->use_ansi_sequences == 0 &&
            colorterm != NULL && colorterm[0] != '\0') {
        state->use_ansi_sequences = 1;
        state->supports_bold = 1;
    }

    if (state->use_ansi_sequences != 0) {
        if (sixel_tty_term_supports_color(term, colorterm) != 0) {
            state->supports_color = 1;
        }
    }

}

SIXELAPI struct sixel_tty_output_state const *
sixel_tty_get_output_state(void)
{
    return &tty_output_state;
}

static SIXELSTATUS
sixel_tty_wait_fd_readable(int fd, int usec, int *is_readable)
{
    SIXELSTATUS status;
#if HAVE_SYS_SELECT_H && !defined(__EMSCRIPTEN__)
    fd_set rfds;
    struct timeval tv;
    int ret;
#endif  /* HAVE_SYS_SELECT_H && !defined(__EMSCRIPTEN__) */

    status = SIXEL_FALSE;
#if HAVE_SYS_SELECT_H && !defined(__EMSCRIPTEN__)
    ret = 0;
    if (is_readable != NULL) {
        *is_readable = 0;
    }
    if (fd < 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (usec < 0) {
        usec = 0;
    }

    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    ret = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (ret < 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        return status;
    }
    if (is_readable != NULL && ret > 0 && FD_ISSET(fd, &rfds)) {
        *is_readable = 1;
    }
    status = SIXEL_OK;
#else
    (void)fd;
    (void)usec;
    if (is_readable != NULL) {
        *is_readable = 0;
    }
#endif

    return status;
}

static int
sixel_tty_parse_cpr_positive_int(char const *response,
                                 size_t response_size,
                                 size_t *cursor,
                                 int *value)
{
    size_t pos;
    int digit;
    int parsed;
    int saw_digit;

    pos = 0u;
    digit = 0;
    parsed = 0;
    saw_digit = 0;

    if (response == NULL || cursor == NULL || value == NULL) {
        return 0;
    }

    pos = *cursor;
    while (pos < response_size) {
        if (response[pos] < '0' || response[pos] > '9') {
            break;
        }
        digit = response[pos] - '0';
        if (parsed > (INT_MAX - digit) / 10) {
            return 0;
        }
        parsed = parsed * 10 + digit;
        saw_digit = 1;
        ++pos;
    }

    if (saw_digit == 0 || parsed <= 0) {
        return 0;
    }

    *cursor = pos;
    *value = parsed;

    return 1;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_parse_cpr_response(int *row,
                             int *col,
                             char const *response,
                             size_t response_size)
{
    size_t index;
    size_t cursor;
    int parsed_col;
    int parsed_row;
    unsigned char ch;

    index = 0u;
    cursor = 0u;
    parsed_col = 0;
    parsed_row = 0;
    ch = 0u;

    if (row == NULL || col == NULL || response == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (index = 0u; index < response_size; ++index) {
        ch = (unsigned char)response[index];
        if (ch == 0x1bu) {
            if (index + 1u >= response_size ||
                response[index + 1u] != '[') {
                continue;
            }
            cursor = index + 2u;
        } else if (ch == 0x9bu) {
            cursor = index + 1u;
        } else {
            continue;
        }

        if (!sixel_tty_parse_cpr_positive_int(response,
                                              response_size,
                                              &cursor,
                                              &parsed_row)) {
            continue;
        }
        if (cursor >= response_size || response[cursor] != ';') {
            continue;
        }
        ++cursor;
        if (!sixel_tty_parse_cpr_positive_int(response,
                                              response_size,
                                              &cursor,
                                              &parsed_col)) {
            continue;
        }
        if (cursor >= response_size || response[cursor] != 'R') {
            continue;
        }

        *row = parsed_row;
        *col = parsed_col;
        return SIXEL_OK;
    }

    return SIXEL_FALSE;
}

#if SIXEL_TTY_HAVE_CURSOR_POSITION_QUERY
static SIXELSTATUS
sixel_tty_query_cursor_position(int fd, int timeout_ms, int *row, int *col)
{
#if HAVE_SYS_SELECT_H && HAVE_UNISTD_H && !defined(__EMSCRIPTEN__)
    SIXELSTATUS status;
    double deadline;
    double now;
    int readable;
    int remaining_usec;
    ssize_t read_size;
    size_t response_size;
    char response[64];

    status = SIXEL_FALSE;
    deadline = 0.0;
    now = 0.0;
    readable = 0;
    remaining_usec = 0;
    read_size = 0;
    response_size = 0u;
    response[0] = '\0';

    if (fd < 0 || row == NULL || col == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (timeout_ms < 0) {
        timeout_ms = 0;
    }

    deadline = sixel_timer_now() + (double)timeout_ms / 1000.0;
    for (;;) {
        now = sixel_timer_now();
        remaining_usec = (int)((deadline - now) * 1000000.0);
        if (remaining_usec < 0) {
            remaining_usec = 0;
        }

        status = sixel_tty_wait_fd_readable(fd,
                                            remaining_usec,
                                            &readable);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (readable == 0) {
            return SIXEL_FALSE;
        }

        read_size = read(fd,
                         response + response_size,
                         sizeof(response) - response_size);
        if (read_size <= 0) {
            return SIXEL_FALSE;
        }
        response_size += (size_t)read_size;

        status = sixel_tty_parse_cpr_response(row,
                                              col,
                                              response,
                                              response_size);
        if (SIXEL_SUCCEEDED(status)) {
            return status;
        }
        if (status != SIXEL_FALSE) {
            return status;
        }
        if (response_size >= sizeof(response)) {
            return SIXEL_FALSE;
        }
        if (sixel_timer_now() >= deadline) {
            return SIXEL_FALSE;
        }
    }
#else
    (void)fd;
    (void)timeout_ms;
    (void)row;
    (void)col;
    return SIXEL_NOT_IMPLEMENTED;
#endif
}
#endif  /* SIXEL_TTY_HAVE_CURSOR_POSITION_QUERY */

#if SIXEL_TTY_HAVE_TERMIOS_CBREAK
static int
sixel_tty_find_abort_restore_slot(int fd)
{
    int index;

    index = 0;
    if (fd < 0) {
        return -1;
    }

    for (index = 0; index < SIXEL_TTY_ABORT_RESTORE_MAX; ++index) {
        if (g_tty_abort_restore_slots[index].active == 0) {
            continue;
        }
        if (g_tty_abort_restore_slots[index].fd == fd) {
            return index;
        }
    }

    return -1;
}

static int
sixel_tty_reserve_abort_restore_slot(int fd)
{
    int index;
    int slot;

    index = 0;
    slot = -1;

    if (fd < 0) {
        return -1;
    }

    slot = sixel_tty_find_abort_restore_slot(fd);
    if (slot >= 0) {
        return slot;
    }

    for (index = 0; index < SIXEL_TTY_ABORT_RESTORE_MAX; ++index) {
        if (g_tty_abort_restore_slots[index].active == 0) {
            slot = index;
            break;
        }
    }

    if (slot < 0) {
        return -1;
    }

    g_tty_abort_restore_slots[slot].active = 1;
    g_tty_abort_restore_slots[slot].fd = fd;
    g_tty_abort_restore_slots[slot].cursor_hidden = 0;
    g_tty_abort_restore_slots[slot].has_saved_termios = 0;
    memset(&g_tty_abort_restore_slots[slot].old_termios,
           0,
           sizeof(g_tty_abort_restore_slots[slot].old_termios));

    return slot;
}

static SIXELSTATUS
sixel_tty_register_cbreak_fd(int fd, struct termios const *old_termios)
{
    int slot;

    slot = -1;

    if (fd < 0 || old_termios == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    slot = sixel_tty_reserve_abort_restore_slot(fd);
    if (slot < 0) {
        return SIXEL_FALSE;
    }

    g_tty_abort_restore_slots[slot].old_termios = *old_termios;
    g_tty_abort_restore_slots[slot].has_saved_termios = 1;
    g_tty_abort_restore_slots[slot].fd = fd;
    g_tty_abort_restore_slots[slot].active = 1;

    return SIXEL_OK;
}

static void
sixel_tty_unregister_cbreak_fd(int fd)
{
    int slot;

    slot = -1;

    if (fd < 0) {
        return;
    }

    slot = sixel_tty_find_abort_restore_slot(fd);
    if (slot < 0) {
        return;
    }

    g_tty_abort_restore_slots[slot].has_saved_termios = 0;
    if (g_tty_abort_restore_slots[slot].cursor_hidden == 0) {
        g_tty_abort_restore_slots[slot].active = 0;
        g_tty_abort_restore_slots[slot].fd = -1;
    }
}

SIXEL_INTERNAL_API int
sixel_tty_is_animation_hide_cursor_enabled(char const *value)
{
    if (value == NULL) {
        return 0;
    }

    if (strcmp(value, "1") == 0) {
        return 1;
    }

    return 0;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_hide_cursor(int fd)
{
    ssize_t written;
    size_t sequence_size;
    int slot;

    written = 0;
    sequence_size = 0u;
    slot = -1;

    if (fd < 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_compat_isatty(fd)) {
        return SIXEL_FALSE;
    }

    slot = sixel_tty_reserve_abort_restore_slot(fd);
    if (slot < 0) {
        return SIXEL_FALSE;
    }
    if (g_tty_abort_restore_slots[slot].cursor_hidden != 0) {
        return SIXEL_OK;
    }

    sequence_size = sizeof(g_tty_hide_cursor_seq) - 1u;
    written = sixel_compat_write(fd, g_tty_hide_cursor_seq, sequence_size);
    if (written != (ssize_t)sequence_size) {
        return SIXEL_FALSE;
    }

    g_tty_abort_restore_slots[slot].cursor_hidden = 1;

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_restore_cursor(int fd)
{
    ssize_t written;
    size_t sequence_size;
    int slot;

    written = 0;
    sequence_size = 0u;
    slot = -1;

    if (fd < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    slot = sixel_tty_find_abort_restore_slot(fd);
    if (slot < 0) {
        return SIXEL_OK;
    }

    if (g_tty_abort_restore_slots[slot].cursor_hidden != 0) {
        sequence_size = sizeof(g_tty_show_cursor_seq) - 1u;
        written = sixel_compat_write(fd,
                                     g_tty_show_cursor_seq,
                                     sequence_size);
        if (written != (ssize_t)sequence_size) {
            return SIXEL_FALSE;
        }
        g_tty_abort_restore_slots[slot].cursor_hidden = 0;
    }

    if (g_tty_abort_restore_slots[slot].has_saved_termios == 0) {
        g_tty_abort_restore_slots[slot].active = 0;
        g_tty_abort_restore_slots[slot].fd = -1;
    }

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_begin_animation_input_guard(void)
{
    SIXELSTATUS status;
    struct termios old_termios;
    struct termios new_termios;
    int slot;

    status = SIXEL_FALSE;
    slot = -1;
    memset(&old_termios, 0, sizeof(old_termios));
    memset(&new_termios, 0, sizeof(new_termios));

    if (!sixel_compat_isatty(STDIN_FILENO)) {
        return SIXEL_FALSE;
    }

    slot = sixel_tty_reserve_abort_restore_slot(STDIN_FILENO);
    if (slot < 0) {
        return SIXEL_FALSE;
    }
    if (g_tty_abort_restore_slots[slot].has_saved_termios != 0) {
        return SIXEL_OK;
    }

    status = sixel_tty_cbreak_fd(STDIN_FILENO, &old_termios, &new_termios);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_end_animation_input_guard(void)
{
    SIXELSTATUS status;
    int slot;

    status = SIXEL_FALSE;
    slot = -1;

    slot = sixel_tty_find_abort_restore_slot(STDIN_FILENO);
    if (slot < 0) {
        return SIXEL_OK;
    }
    if (g_tty_abort_restore_slots[slot].has_saved_termios == 0) {
        return SIXEL_OK;
    }

    status = sixel_tty_restore_fd(
        STDIN_FILENO,
        &g_tty_abort_restore_slots[slot].old_termios);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_restore_animation_cursor_to_bottom(int outfd, int height)
{
    SIXELSTATUS status;
    ssize_t written;
#if defined(TIOCGWINSZ)
    struct winsize size;
    int nwrite;
    int cellheight;
    char sequence[64];
#endif

    status = SIXEL_FALSE;
    written = 0;
#if defined(TIOCGWINSZ)
    memset(&size, 0, sizeof(size));
    nwrite = 0;
    cellheight = 0;
    memset(sequence, 0, sizeof(sequence));
#endif

    if (outfd < 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_compat_isatty(outfd)) {
        return SIXEL_FALSE;
    }

#if defined(TIOCGWINSZ)
    if (ioctl(outfd, TIOCGWINSZ, &size) == 0
        && size.ws_ypixel > 0
        && size.ws_row > 0) {
        cellheight = (height * size.ws_row + size.ws_ypixel - 1)
            / size.ws_ypixel;
        if (cellheight < 1) {
            cellheight = 1;
        }
        nwrite = sixel_compat_snprintf(sequence,
                                       sizeof(sequence),
                                       "\0338\033[%dB\r",
                                       cellheight);
        if (nwrite <= 0 || nwrite >= (int)sizeof(sequence)) {
            return SIXEL_FALSE;
        }
        written = sixel_compat_write(outfd, sequence, (size_t)nwrite);
        if (written != (ssize_t)nwrite) {
            return SIXEL_FALSE;
        }
        return SIXEL_OK;
    }
#endif

    written = sixel_compat_write(outfd, "\0338\r", 3u);
    if (written != 3) {
        return SIXEL_FALSE;
    }

    status = SIXEL_OK;
    return status;
}

static SIXELSTATUS
sixel_tty_cbreak_fd(int fd,
                    struct termios *old_termios,
                    struct termios *new_termios)
{
    SIXELSTATUS status;
    int ret;

    status = SIXEL_FALSE;
    ret = 0;

    if (fd < 0 || old_termios == NULL || new_termios == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    ret = tcgetattr(fd, old_termios);
    if (ret != 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        return status;
    }

    (void)memcpy(new_termios, old_termios, sizeof(*old_termios));
    new_termios->c_lflag &= (tcflag_t)~(ECHO | ICANON);
    new_termios->c_cc[VMIN] = 1;
    new_termios->c_cc[VTIME] = 0;

    ret = tcsetattr(fd, TCSAFLUSH, new_termios);
    if (ret != 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        return status;
    }

    (void)sixel_tty_register_cbreak_fd(fd, old_termios);

    status = SIXEL_OK;
    return status;
}

static SIXELSTATUS
sixel_tty_restore_fd(int fd, struct termios *old_termios)
{
    SIXELSTATUS status;
    int ret;

    status = SIXEL_FALSE;
    ret = 0;

    if (fd < 0 || old_termios == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    ret = tcsetattr(fd, TCSAFLUSH, old_termios);
    if (ret != 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        return status;
    }

    sixel_tty_unregister_cbreak_fd(fd);

    status = SIXEL_OK;
    return status;
}

SIXEL_INTERNAL_API void
sixel_tty_restore_cbreak_for_abort(void)
{
    int index;
    int fd;
    ssize_t written;
    size_t sequence_size;
    int has_saved_termios;
    struct termios old_termios;

    index = 0;
    fd = -1;
    written = 0;
    sequence_size = 0u;
    has_saved_termios = 0;
    memset(&old_termios, 0, sizeof(old_termios));

    /*
     * Abort handlers can interrupt normal cleanup while the terminal still
     * runs in cbreak mode or with hidden cursor state. Restore both resources
     * best-effort so the shell is usable after abnormal termination.
     */
    for (index = 0; index < SIXEL_TTY_ABORT_RESTORE_MAX; ++index) {
        if (g_tty_abort_restore_slots[index].active == 0) {
            continue;
        }

        fd = g_tty_abort_restore_slots[index].fd;
        has_saved_termios =
            g_tty_abort_restore_slots[index].has_saved_termios;
        old_termios = g_tty_abort_restore_slots[index].old_termios;
        if (g_tty_abort_restore_slots[index].cursor_hidden != 0 && fd >= 0) {
            sequence_size = sizeof(g_tty_show_cursor_seq) - 1u;
            written = write(fd, g_tty_show_cursor_seq, sequence_size);
            (void)written;
            g_tty_abort_restore_slots[index].cursor_hidden = 0;
        }
        g_tty_abort_restore_slots[index].has_saved_termios = 0;
        g_tty_abort_restore_slots[index].active = 0;
        g_tty_abort_restore_slots[index].fd = -1;

        if (fd >= 0 && has_saved_termios != 0) {
            (void)tcsetattr(fd, TCSAFLUSH, &old_termios);
        }
    }
}

SIXELSTATUS
sixel_tty_cbreak(struct termios *old_termios, struct termios *new_termios)
{
    SIXELSTATUS status = SIXEL_FALSE;
    status = sixel_tty_cbreak_fd(STDIN_FILENO, old_termios, new_termios);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_tty_cbreak: tcsetattr() failed.");
    }
    return status;
}
#endif  /* SIXEL_TTY_HAVE_TERMIOS_CBREAK */

#if !SIXEL_TTY_HAVE_TERMIOS_CBREAK
SIXEL_INTERNAL_API int
sixel_tty_is_animation_hide_cursor_enabled(char const *value)
{
    if (value == NULL) {
        return 0;
    }

    if (strcmp(value, "1") == 0) {
        return 1;
    }

    return 0;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_hide_cursor(int fd)
{
    ssize_t written;
    size_t sequence_size;

    written = 0;
    sequence_size = 0u;

    if (fd < 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_compat_isatty(fd)) {
        return SIXEL_FALSE;
    }

    sequence_size = sizeof(g_tty_hide_cursor_seq) - 1u;
    written = sixel_compat_write(fd, g_tty_hide_cursor_seq, sequence_size);
    if (written != (ssize_t)sequence_size) {
        return SIXEL_FALSE;
    }

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_restore_cursor(int fd)
{
    ssize_t written;
    size_t sequence_size;

    written = 0;
    sequence_size = 0u;

    if (fd < 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_compat_isatty(fd)) {
        return SIXEL_OK;
    }

    sequence_size = sizeof(g_tty_show_cursor_seq) - 1u;
    written = sixel_compat_write(fd, g_tty_show_cursor_seq, sequence_size);
    if (written != (ssize_t)sequence_size) {
        return SIXEL_FALSE;
    }

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_begin_animation_input_guard(void)
{
    return SIXEL_FALSE;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_end_animation_input_guard(void)
{
    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tty_restore_animation_cursor_to_bottom(int outfd, int height)
{
    (void)outfd;
    (void)height;
    return SIXEL_FALSE;
}

SIXEL_INTERNAL_API void
sixel_tty_restore_cbreak_for_abort(void)
{
}
#endif  /* !SIXEL_TTY_HAVE_TERMIOS_CBREAK */


#if SIXEL_TTY_HAVE_TERMIOS_CBREAK
SIXELSTATUS
sixel_tty_restore(struct termios *old_termios)
{
    SIXELSTATUS status = SIXEL_FALSE;

    sixel_trace_topic_message("lifecycle",
        "tty restore begin: tcsetattr(TCSAFLUSH)");
    status = sixel_tty_restore_fd(STDIN_FILENO, old_termios);
    if (SIXEL_FAILED(status)) {
        sixel_trace_topic_message("lifecycle",
            "tty restore failed: errno=%d",
            errno);
        sixel_helper_set_additional_message(
            "sixel_tty_restore: tcsetattr() failed.");
        goto end;
    }

    sixel_trace_topic_message("lifecycle",
        "tty restore end: success");
end:
    return status;
}
#endif  /* SIXEL_TTY_HAVE_TERMIOS_CBREAK */


SIXELSTATUS
sixel_tty_wait_stdin(int usec)
{
    int readable;
    SIXELSTATUS status = SIXEL_FALSE;

    readable = 0;
    status = sixel_tty_wait_fd_readable(STDIN_FILENO, usec, &readable);
    if (SIXEL_FAILED(status)) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "sixel_tty_wait_stdin: select() failed.");
        return status;
    }

    return SIXEL_OK;
}

SIXELSTATUS
sixel_tty_query_osc11_bgcolor_with_drain(
    unsigned char *bgcolor,
    int timeout_ms,
    sixel_tty_query_stop_function should_stop,
    void *context)
{
#if HAVE_FCNTL_H && HAVE_SYS_SELECT_H && SIXEL_TTY_HAVE_TERMIOS_CBREAK \
    && !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    static char const query[] = "\033]11;?\007";
    SIXELSTATUS status;
    struct termios old_termios;
    struct termios new_termios;
    int ttyfd;
    ssize_t written;
    ssize_t read_size;
    int readable;
    int remaining_ms;
    int slice_ms;
    int wait_usec;
    int raw_active;
    int keep_draining;
    int stop_requested;
    size_t response_size;
    char response[512];

    status = SIXEL_FALSE;
    ttyfd = -1;
    written = 0;
    read_size = 0;
    readable = 0;
    remaining_ms = 0;
    slice_ms = 0;
    wait_usec = 0;
    raw_active = 0;
    keep_draining = 0;
    stop_requested = 0;
    response_size = 0u;
    response[0] = '\0';

    if (bgcolor == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (timeout_ms < 0) {
        timeout_ms = 0;
    }
    if (should_stop != NULL) {
        keep_draining = 1;
    }
    remaining_ms = timeout_ms;

    ttyfd = sixel_compat_open("/dev/tty", O_RDWR);
    if (ttyfd < 0) {
        return SIXEL_FALSE;
    }
    if (!sixel_compat_isatty(ttyfd)) {
        goto cleanup;
    }

    status = sixel_tty_cbreak_fd(ttyfd, &old_termios, &new_termios);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    raw_active = 1;

    written = sixel_compat_write(ttyfd, query, sizeof(query) - 1u);
    if (written != (ssize_t)(sizeof(query) - 1u)) {
        status = SIXEL_FALSE;
        goto cleanup;
    }

    for (;;) {
        stop_requested = 0;
        if (keep_draining != 0) {
            stop_requested = should_stop(context);
        }

        wait_usec = 0;
        if (stop_requested != 0) {
            /* Drain whatever is already queued before leaving. */
            wait_usec = 0;
            slice_ms = 0;
        } else if (remaining_ms > 0) {
            /*
             * Poll in small slices so a partial OSC response can be consumed
             * across multiple reads while still honoring the total timeout.
             */
            slice_ms = remaining_ms;
            if (slice_ms > 10) {
                slice_ms = 10;
            }
            wait_usec = slice_ms * 1000;
        } else {
            slice_ms = 0;
            if (keep_draining != 0) {
                wait_usec = 10 * 1000;
            }
        }
        status = sixel_tty_wait_fd_readable(ttyfd, wait_usec, &readable);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        if (!readable) {
            if (stop_requested != 0) {
                goto cleanup;
            }
            if (remaining_ms <= 0 && keep_draining == 0) {
                status = SIXEL_FALSE;
                goto cleanup;
            }
            if (remaining_ms > 0) {
                remaining_ms -= slice_ms;
            }
            continue;
        }
        if (response_size + 1u >= sizeof(response)) {
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }
        read_size = read(ttyfd,
                         response + response_size,
                         sizeof(response) - response_size - 1u);
        if (read_size <= 0) {
            status = SIXEL_FALSE;
            goto cleanup;
        }
        response_size += (size_t)read_size;
        response[response_size] = '\0';

        status = sixel_tty_parse_osc11_response(bgcolor,
                                                response,
                                                response_size);
        if (SIXEL_SUCCEEDED(status)) {
            goto cleanup;
        }
        if (status == SIXEL_BAD_ALLOCATION) {
            goto cleanup;
        }
    }

cleanup:
    if (raw_active != 0) {
        (void)sixel_tty_restore_fd(ttyfd, &old_termios);
    }
    if (ttyfd >= 0) {
        (void)sixel_compat_close(ttyfd);
    }
    return status;
#else
    (void)bgcolor;
    (void)timeout_ms;
    (void)should_stop;
    (void)context;
    return SIXEL_NOT_IMPLEMENTED;
#endif
}

SIXELSTATUS
sixel_tty_query_osc11_bgcolor(unsigned char *bgcolor, int timeout_ms)
{
    return sixel_tty_query_osc11_bgcolor_with_drain(bgcolor,
                                                    timeout_ms,
                                                    NULL,
                                                    NULL);
}


SIXELSTATUS
sixel_tty_scroll(
    sixel_write_function f_write,
    void *priv,
    int outfd,
    int height,
    int is_animation)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int nwrite;
#if SIXEL_TTY_HAVE_CURSOR_POSITION_QUERY
    struct winsize size = {0, 0, 0, 0};
    struct termios old_termios;
    struct termios new_termios;
    SIXELSTATUS restore_status;
    int row = 0;
    int col = 0;
    int cellheight;
    int scroll;
    int raw_active;
    char buffer[256];
    int result;

    restore_status = SIXEL_FALSE;
    raw_active = 0;

    /* confirm I/O file descriptors are tty devices */
    if (!sixel_compat_isatty(STDIN_FILENO)
        || !sixel_compat_isatty(outfd)) {
        /* set cursor position to top-left */
        nwrite = f_write("\033[H", 3, priv);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_tty_scroll: f_write() failed.");
            goto end;
        }
        status = SIXEL_OK;
        goto end;
    }

    /* request terminal size to tty device with TIOCGWINSZ ioctl */
    result = ioctl(outfd, TIOCGWINSZ, &size);
    if (result != 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message("ioctl() failed.");
        goto end;
    }

    /* if we can not retrieve terminal pixel size over TIOCGWINSZ ioctl,
       return immediately */
    if (size.ws_ypixel <= 0) {
        nwrite = f_write("\033[H", 3, priv);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_tty_scroll: f_write() failed.");
            goto end;
        }
        status = SIXEL_OK;
        goto end;
    }

    /* if input source is animation and frame No. is more than 1,
       output DECSC sequence */
    if (is_animation) {
        nwrite = f_write("\0338", 2, priv);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_tty_scroll: f_write() failed.");
            goto end;
        }
        status = SIXEL_OK;
        goto end;
    }

    /* set the terminal to cbreak mode */
    status = sixel_tty_cbreak(&old_termios, &new_termios);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    raw_active = 1;

    /* request cursor position report */
    nwrite = f_write("\033[6n", 4, priv);
    if (nwrite < 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "sixel_tty_scroll: f_write() failed.");
        goto end;
    }

    /*
     * Wait for a cursor position report with a hard deadline.  scanf() is
     * intentionally not used here because a timeout, a partial CPR, or an
     * unrelated byte in the terminal input stream must not leave stdin
     * blocked in a format-string read.
     */
    status = sixel_tty_query_cursor_position(STDIN_FILENO, 1000, &row, &col);
    if (SIXEL_FAILED(status)) {
        /*
         * If we can't get any response from the terminal, move the cursor to
         * (1, 1) as the historical fallback.
         */
        nwrite = f_write("\033[H", 3, priv);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_tty_scroll: f_write() failed.");
            goto end;
        }
        status = SIXEL_OK;
        goto end;
    }

    /* calculate scrolling amount in pixels */
    cellheight = height * size.ws_row / size.ws_ypixel + 1;
    scroll = cellheight + row - size.ws_row + 1;
    if (scroll > 0) {
        nwrite = sixel_compat_snprintf(
            buffer,
            sizeof(buffer),
            "\033[%dS\033[%dA",
            scroll,
            scroll);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_tty_scroll: command format failed.");
        }
        nwrite = f_write(buffer, (int)strlen(buffer), priv);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_tty_scroll: f_write() failed.");
            goto end;
        }
    }

    /* emit DECSC sequence */
    nwrite = f_write("\0337", 2, priv);
    if (nwrite < 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "sixel_tty_scroll: f_write() failed.");
        goto end;
    }
#else  /* simple scroll fallback */
    (void) outfd;
    (void) height;
    (void) is_animation;
    nwrite = f_write("\033[H", 3, priv);
    if (nwrite < 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "sixel_tty_scroll: f_write() failed.");
        goto end;
    }
#endif  /* SIXEL_TTY_HAVE_CURSOR_POSITION_QUERY */

    status = SIXEL_OK;

end:
#if SIXEL_TTY_HAVE_CURSOR_POSITION_QUERY
    if (raw_active != 0) {
        restore_status = sixel_tty_restore(&old_termios);
        if (SIXEL_FAILED(restore_status) && SIXEL_SUCCEEDED(status)) {
            status = restore_status;
        }
    }
#endif
    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
