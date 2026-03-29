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

/*
 * Cache describing the capabilities of the active output device.
 * The struct lives in static storage because the helper routines are
 * frequently used from the CLI tools without an explicit lifecycle.
 */
static struct sixel_tty_output_state tty_output_state = {0, 0, 0, 0};

static int
sixel_tty_term_supports_ansi(const char *term);

static int
sixel_tty_term_supports_color(const char *term, const char *colorterm);

static char *
sixel_tty_strdup(char const *text);

static SIXELSTATUS
sixel_tty_wait_fd_readable(int fd, int usec, int *is_readable);

#if HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY
#define SIXEL_TTY_ABORT_RESTORE_MAX 8

typedef struct sixel_tty_abort_restore_slot {
    volatile int active;
    int fd;
    struct termios old_termios;
} sixel_tty_abort_restore_slot_t;

static sixel_tty_abort_restore_slot_t
    g_tty_abort_restore_slots[SIXEL_TTY_ABORT_RESTORE_MAX];

static SIXELSTATUS
sixel_tty_register_cbreak_fd(int fd, struct termios const *old_termios);

static void
sixel_tty_unregister_cbreak_fd(int fd);

static SIXELSTATUS
sixel_tty_cbreak_fd(int fd,
                    struct termios *old_termios,
                    struct termios *new_termios);

static SIXELSTATUS
sixel_tty_restore_fd(int fd, struct termios *old_termios);
#endif  /* HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY */

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

#if HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY
static SIXELSTATUS
sixel_tty_register_cbreak_fd(int fd, struct termios const *old_termios)
{
    int index;
    int slot;

    index = 0;
    slot = -1;

    if (fd < 0 || old_termios == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (index = 0; index < SIXEL_TTY_ABORT_RESTORE_MAX; ++index) {
        if (g_tty_abort_restore_slots[index].active != 0) {
            if (g_tty_abort_restore_slots[index].fd == fd) {
                slot = index;
                break;
            }
            continue;
        }
        if (slot < 0) {
            slot = index;
        }
    }

    if (slot < 0) {
        return SIXEL_FALSE;
    }

    g_tty_abort_restore_slots[slot].old_termios = *old_termios;
    g_tty_abort_restore_slots[slot].fd = fd;
    g_tty_abort_restore_slots[slot].active = 1;

    return SIXEL_OK;
}

static void
sixel_tty_unregister_cbreak_fd(int fd)
{
    int index;

    index = 0;

    if (fd < 0) {
        return;
    }

    for (index = 0; index < SIXEL_TTY_ABORT_RESTORE_MAX; ++index) {
        if (g_tty_abort_restore_slots[index].active == 0) {
            continue;
        }
        if (g_tty_abort_restore_slots[index].fd != fd) {
            continue;
        }
        g_tty_abort_restore_slots[index].active = 0;
        g_tty_abort_restore_slots[index].fd = -1;
    }
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
    struct termios old_termios;

    index = 0;
    fd = -1;
    memset(&old_termios, 0, sizeof(old_termios));

    /*
     * Abort handlers can interrupt normal cleanup while the terminal still
     * runs in cbreak mode. Restore every tracked descriptor best-effort so
     * the shell is usable after abnormal termination.
     */
    for (index = 0; index < SIXEL_TTY_ABORT_RESTORE_MAX; ++index) {
        if (g_tty_abort_restore_slots[index].active == 0) {
            continue;
        }

        fd = g_tty_abort_restore_slots[index].fd;
        old_termios = g_tty_abort_restore_slots[index].old_termios;
        g_tty_abort_restore_slots[index].active = 0;
        g_tty_abort_restore_slots[index].fd = -1;

        if (fd >= 0) {
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
#endif  /* HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY */

#if !HAVE_TERMIOS_H || !HAVE_SYS_IOCTL_H || !HAVE_ISATTY
SIXEL_INTERNAL_API void
sixel_tty_restore_cbreak_for_abort(void)
{
}
#endif  /* !HAVE_TERMIOS_H || !HAVE_SYS_IOCTL_H || !HAVE_ISATTY */


#if HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY
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
#endif  /* HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY */


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
#if HAVE_FCNTL_H && HAVE_SYS_SELECT_H && HAVE_TERMIOS_H && HAVE_ISATTY \
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
        sixel_tty_unregister_cbreak_fd(ttyfd);
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
#if HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY \
    && !defined(__EMSCRIPTEN__) && defined(TIOCGWINSZ)
    struct winsize size = {0, 0, 0, 0};
    struct termios old_termios;
    struct termios new_termios;
    int row = 0;
    int col = 0;
    int cellheight;
    int scroll;
    char buffer[256];
    int result;

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

    /* request cursor position report */
    nwrite = f_write("\033[6n", 4, priv);
    if (nwrite < 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "sixel_tty_scroll: f_write() failed.");
        goto end;
    }

    /* wait cursor position report */
    if (SIXEL_FAILED(sixel_tty_wait_stdin(1000 * 1000))) { /* wait up to 1 sec */
        /* If we can't get any response from the terminal,
         * move cursor to (1, 1). */
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

    /* scan cursor position report */
    if (scanf("\033[%d;%dR", &row, &col) != 2) {
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

    /* restore the terminal mode */
    status = sixel_tty_restore(&old_termios);
    if (SIXEL_FAILED(status)) {
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
#else  /* mingw */
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
#endif  /* HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY && !defined(__EMSCRIPTEN__) */

    status = SIXEL_OK;

end:
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
