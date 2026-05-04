/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2018 Hayaki Saito
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

#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_MEMORY_H
# include <memory.h>
#endif  /* HAVE_MEMORY_H */
#ifdef HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#ifdef SIXEL_DISABLE_EXT_LIBS
# undef HAVE_LIBCURL
# undef HAVE_JPEG
# undef HAVE_LIBPNG
# undef HAVE_GDK_PIXBUF2
# undef HAVE_GD
#endif

#ifdef HAVE_LIBCURL
# include <curl/curl.h>
#endif  /* HAVE_LIBCURL */

#include <sixel.h>
#include "status.h"

#if !defined(SIXEL_STATUS_HAVE_TTY_HELPERS)
/*
 * Some auxiliary targets (for example the macOS Quick Look helpers)
 * build src/status.c without linking src/tty.c.  The flag lets those
 * targets drop the tty dependency while the main library keeps it.
 */
#define SIXEL_STATUS_HAVE_TTY_HELPERS 1
#endif

#if SIXEL_STATUS_HAVE_TTY_HELPERS
# include "tty.h"
#endif
#include "compat_stub.h"

/* Keep SIZE_MAX available even on strict C99 environments. */
#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#define SIXEL_MESSAGE_OK                    ("succeeded")
#define SIXEL_MESSAGE_FALSE                 ("unexpected error (SIXEL_FALSE)");
#define SIXEL_MESSAGE_UNEXPECTED            ("unexpected error")
#define SIXEL_MESSAGE_INTERRUPTED           ("interrupted by a signal")
#define SIXEL_MESSAGE_BAD_ALLOCATION        ("runtime error: bad allocation error")
#define SIXEL_MESSAGE_BAD_ARGUMENT          ("runtime error: bad argument detected")
#define SIXEL_MESSAGE_BAD_INPUT             ("runtime error: bad input detected")
#define SIXEL_MESSAGE_BAD_INTEGER_OVERFLOW  ("runtime error: integer overflow")
#define SIXEL_MESSAGE_BAD_CLIPBOARD         \
    ("runtime error: clipboard payload unavailable")
#define SIXEL_MESSAGE_LOADER_FAILED         \
    ("runtime error: unable to decode input with available loaders")
#define SIXEL_MESSAGE_RUNTIME_ERROR         ("runtime error")
#define SIXEL_MESSAGE_LOGIC_ERROR           ("logic error")
#define SIXEL_MESSAGE_NOT_IMPLEMENTED       ("feature error: not implemented")
#define SIXEL_MESSAGE_FEATURE_ERROR         ("feature error")
#define SIXEL_MESSAGE_STBI_ERROR            ("stb_image error")
#define SIXEL_MESSAGE_STBIW_ERROR           ("stb_image_write error")
#define SIXEL_MESSAGE_JPEG_ERROR            ("libjpeg error")
#define SIXEL_MESSAGE_PNG_ERROR             ("libpng error")
#define SIXEL_MESSAGE_TIFF_ERROR            ("libtiff error")
#define SIXEL_MESSAGE_GDK_ERROR             ("GDK error")
#define SIXEL_MESSAGE_GD_ERROR              ("GD error")
#define SIXEL_MESSAGE_COM_ERROR             ("COM error")
#define SIXEL_MESSAGE_WIC_ERROR             ("WIC error")

void
sixel_debugf(char const *fmt, ...)
{
#if HAVE_DEBUG
    enum { message_length = 256 };
    char message[message_length];
    va_list args;
    int nwrite;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    nwrite = sixel_compat_vsnprintf(message,
                                    sizeof(message),
                                    fmt,
                                    args);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    va_end(args);

    if (nwrite > 0) {
        sixel_helper_set_additional_message(message);
    }
#else
    (void)fmt;
#endif
}

enum {
    SIXEL_STATUS_MESSAGE_INITIAL_CAPACITY = 4096
};

#if defined(__PCC__) || defined(__TINYC__)
# define SIXEL_STATUS_NO_TLS_COMPILER 1
#else
# define SIXEL_STATUS_NO_TLS_COMPILER 0
#endif

#if defined(_MSC_VER)
# if defined(_MT)
#  define SIXEL_STATUS_TLS __declspec(thread)
# else
#  define SIXEL_STATUS_TLS
# endif
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !SIXEL_STATUS_NO_TLS_COMPILER
# define SIXEL_STATUS_TLS _Thread_local
#elif (defined(__GNUC__) || defined(__clang__)) \
    && !SIXEL_STATUS_NO_TLS_COMPILER
# define SIXEL_STATUS_TLS __thread
#else
# define SIXEL_STATUS_TLS
#endif

typedef struct sixel_status_message_store {
    char markup_storage[SIXEL_STATUS_MESSAGE_INITIAL_CAPACITY];
    char render_storage[SIXEL_STATUS_MESSAGE_INITIAL_CAPACITY];
    char *markup_buffer;
    char *render_buffer;
    size_t markup_capacity;
    size_t render_capacity;
    int initialized;
} sixel_status_message_store_t;

/*
 * The public getter returns a borrowed pointer.  Keeping both the raw markup
 * and rendered text in thread-local storage prevents parallel workers from
 * racing on a process-wide buffer or invalidating another thread's return
 * value.
 */
static SIXEL_STATUS_TLS sixel_status_message_store_t status_message_store;

#undef SIXEL_STATUS_TLS
#undef SIXEL_STATUS_NO_TLS_COMPILER

enum sixel_status_markup_attr {
    SIXEL_STATUS_ATTR_NONE = 0,
    SIXEL_STATUS_ATTR_BOLD,
    SIXEL_STATUS_ATTR_ERROR,
    SIXEL_STATUS_ATTR_WARNING
};

static size_t
sixel_status_utf8_expected_length(unsigned char byte);

static size_t
sixel_status_utf8_trim_length(const char *message, size_t length);

static size_t
sixel_status_append_literal(char *buffer,
                            size_t buffer_size,
                            size_t index,
                            char value);

static size_t
sixel_status_append_sequence(char *buffer,
                             size_t buffer_size,
                             size_t index,
                             const char *sequence);

static size_t
sixel_status_render_markup(const char *source,
                           char *destination,
                           size_t destination_size);

static int
sixel_status_reserve_buffer(char **buffer,
                            size_t *capacity,
                            char *static_storage,
                            size_t static_capacity,
                            size_t required_size);

static int
sixel_status_force_colors_enabled(void);

static sixel_status_message_store_t *
sixel_status_get_message_store(void);

/*
 * Markup overview.
 * +---------+------------------------------+-----------------------------+
 * | Token   | Meaning                      | Notes                       |
 * +---------+------------------------------+-----------------------------+
 * | \\fB ... | Bold text                   | Closed by \\fP              |
 * | \\fE ... | Error highlight (red)       | Closed by \\fP              |
 * | \\fW ... | Warning highlight (yellow)  | Closed by \\fP              |
 * | \\fP     | Reset attributes            | Applies to active token     |
 * | \\\\     | Literal backslash           |                             |
 * | \\f\\\\  | Literal form-feed marker    | Emits a single '\\f'        |
 * +---------+------------------------------+-----------------------------+
 */

/* set detailed error message */
SIXELAPI void
sixel_helper_set_additional_message(
    const char      /* in */  *message         /* error message */
)
{
    sixel_status_message_store_t *store;
    size_t length;
    size_t required_size;
    int reserve_status;

    store = sixel_status_get_message_store();
    length = 0u;
    required_size = 0u;
    reserve_status = -1;
    if (message == NULL) {
        if (store->markup_buffer != NULL && store->markup_capacity > 0u) {
            store->markup_buffer[0] = '\0';
        }
        if (store->render_buffer != NULL && store->render_capacity > 0u) {
            store->render_buffer[0] = '\0';
        }
        return;
    }

    length = strlen(message);
    required_size = length + 1u;
    reserve_status = sixel_status_reserve_buffer(
        &store->markup_buffer,
        &store->markup_capacity,
        store->markup_storage,
        sizeof(store->markup_storage),
        required_size);
    if (reserve_status != 0) {
        if (store->markup_capacity == 0u) {
            return;
        }
        length = store->markup_capacity - 1u;
        length = sixel_status_utf8_trim_length(message,
                                               length);
    }

    if (store->markup_buffer == NULL) {
        return;
    }

    memcpy(store->markup_buffer, message, length);
    store->markup_buffer[length] = '\0';
    if (store->render_buffer != NULL && store->render_capacity > 0u) {
        store->render_buffer[0] = '\0';
    }
}


/* get detailed error message */
SIXELAPI char const *
sixel_helper_get_additional_message(void)
{
    sixel_status_message_store_t *store;
    size_t markup_length;
    size_t required_size;

    store = sixel_status_get_message_store();
    markup_length = 0u;
    required_size = 0u;
    if (store->markup_buffer == NULL) {
        return "";
    }

    markup_length = strlen(store->markup_buffer);
    required_size = markup_length + 1u;
    if (markup_length <= (SIZE_MAX - 64u) / 4u) {
        required_size = markup_length * 4u + 64u;
    }
    (void)sixel_status_reserve_buffer(
        &store->render_buffer,
        &store->render_capacity,
        store->render_storage,
        sizeof(store->render_storage),
        required_size);
    if (store->render_buffer == NULL || store->render_capacity == 0u) {
        return "";
    }

    (void)sixel_status_render_markup(store->markup_buffer,
                                     store->render_buffer,
                                     store->render_capacity);
    return store->render_buffer;
}

static sixel_status_message_store_t *
sixel_status_get_message_store(void)
{
    sixel_status_message_store_t *store;

    store = &status_message_store;
    if (store->initialized == 0) {
        store->markup_buffer = store->markup_storage;
        store->render_buffer = store->render_storage;
        store->markup_capacity = sizeof(store->markup_storage);
        store->render_capacity = sizeof(store->render_storage);
        store->markup_storage[0] = '\0';
        store->render_storage[0] = '\0';
        store->initialized = 1;
    }

    return store;
}

static int
sixel_status_reserve_buffer(char **buffer,
                            size_t *capacity,
                            char *static_storage,
                            size_t static_capacity,
                            size_t required_size)
{
    char *grown;
    size_t target;

    grown = NULL;
    target = 0u;
    if (buffer == NULL || capacity == NULL || required_size == 0u) {
        return -1;
    }
    if (*buffer != NULL && *capacity >= required_size) {
        return 0;
    }

    target = *capacity;
    if (target == 0u) {
        target = static_capacity;
        if (target == 0u) {
            target = 64u;
        }
    }
    while (target < required_size) {
        if (target > SIZE_MAX / 2u) {
            target = required_size;
            break;
        }
        target *= 2u;
    }

    if (*buffer == static_storage) {
        grown = (char *)malloc(target);
        if (grown == NULL) {
            return -1;
        }
        /*
         * Guard memcpy source for MSVC /analyze: static_storage can be NULL
         * when caller does not provide inline storage.
         */
        if (static_storage != NULL && static_capacity > 0u) {
            memcpy(grown, static_storage, static_capacity);
        } else {
            grown[0] = '\0';
        }
    } else {
        grown = (char *)realloc(*buffer, target);
        if (grown == NULL) {
            return -1;
        }
    }

    *buffer = grown;
    *capacity = target;
    return 0;
}

void
sixel_status_render_markup_text(char const *source,
                                char *destination,
                                size_t destination_size)
{
    if (destination == NULL || destination_size == 0) {
        return;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    (void)sixel_status_render_markup(source,
                                     destination,
                                     destination_size);
}

static size_t
sixel_status_utf8_expected_length(unsigned char byte)
{
    size_t length;

    length = 1;

    if ((byte & 0x80u) == 0) {
        length = 1;
    } else if ((byte & 0xE0u) == 0xC0u) {
        length = 2;
    } else if ((byte & 0xF0u) == 0xE0u) {
        length = 3;
    } else if ((byte & 0xF8u) == 0xF0u) {
        length = 4;
    }

    return length;
}

static size_t
sixel_status_utf8_trim_length(const char *message, size_t length)
{
    size_t index;
    size_t available;
    size_t required;
    unsigned char byte;

    index = length;

    while (index > 0) {
        byte = (unsigned char)message[index - 1];
        if ((byte & 0xC0u) != 0x80u) {
            required = sixel_status_utf8_expected_length(byte);
            available = length - (index - 1);
            if (available >= required) {
                return length;
            }
            length = index - 1;
            index = length;
            continue;
        }
        --index;
    }

    return length;
}

static size_t
sixel_status_append_literal(char *buffer,
                            size_t buffer_size,
                            size_t index,
                            char value)
{
    size_t remaining;

    if (buffer_size == 0) {
        return 0;
    }

    if (index >= buffer_size) {
        return buffer_size;
    }

    remaining = buffer_size - index;
    if (remaining <= 1) {
        buffer[buffer_size - 1] = '\0';
        return buffer_size;
    }

    buffer[index] = value;
    ++index;
    buffer[index] = '\0';

    return index;
}

static size_t
sixel_status_append_sequence(char *buffer,
                             size_t buffer_size,
                             size_t index,
                             const char *sequence)
{
    size_t length;
    size_t remaining;

    if (buffer_size == 0) {
        return 0;
    }

    if (index >= buffer_size) {
        return buffer_size;
    }

    length = strlen(sequence);
    remaining = buffer_size - index;

    if (length + 1 > remaining) {
        buffer[buffer_size - 1] = '\0';
        return buffer_size;
    }

    memcpy(buffer + index, sequence, length);
    index += length;
    buffer[index] = '\0';

    return index;
}

static size_t
sixel_status_render_markup(const char *source,
                           char *destination,
                           size_t destination_size)
{
    size_t index;
    const char *cursor;
    enum sixel_status_markup_attr active_attr;
    int use_sequences;
    int have_bold;
    int have_color;
    int sequence_active;
    size_t appended;
    char token;
    const char *start_sequence;
    enum sixel_status_markup_attr target_attr;
    int force_colors;

    index = 0;

    if (destination_size == 0) {
        return 0;
    }

    destination[0] = '\0';
    cursor = source;
    use_sequences = 0;
    have_bold = 0;
    have_color = 0;
    force_colors = sixel_status_force_colors_enabled();

    if (force_colors != 0) {
        use_sequences = 1;
        have_bold = 1;
        have_color = 1;
    }

#if SIXEL_STATUS_HAVE_TTY_HELPERS
    if (force_colors == 0) {
        const struct sixel_tty_output_state *tty;

        tty = sixel_tty_get_output_state();
        if (tty != NULL) {
            if (tty->is_tty != 0 && tty->use_ansi_sequences != 0) {
                use_sequences = 1;
            }
            if (tty->supports_bold != 0) {
                have_bold = 1;
            }
            if (tty->supports_color != 0) {
                have_color = 1;
            }
        }
    }
#endif  /* SIXEL_STATUS_HAVE_TTY_HELPERS */

    active_attr = SIXEL_STATUS_ATTR_NONE;
    sequence_active = 0;

    while (cursor != NULL && cursor[0] != '\0') {
        if (cursor[0] == '\\') {
            if (cursor[1] == '\\') {
                appended = sixel_status_append_literal(destination,
                                                       destination_size,
                                                       index,
                                                       '\\');
                index = appended;
                if (index >= destination_size) {
                    break;
                }
                cursor += 2;
                continue;
            }

            if (cursor[1] == 'f') {
                if (cursor[2] == '\\' && cursor[3] == '\\') {
                    appended = sixel_status_append_literal(destination,
                                                           destination_size,
                                                           index,
                                                           '\f');
                    index = appended;
                    if (index >= destination_size) {
                        break;
                    }
                    cursor += 4;
                    continue;
                }

                if (cursor[2] == 'P') {
                    if (sequence_active != 0 && use_sequences != 0) {
                        appended = sixel_status_append_sequence(
                            destination,
                            destination_size,
                            index,
                            "\x1b[0m");
                        index = appended;
                        if (index >= destination_size) {
                            break;
                        }
                        sequence_active = 0;
                    }
                    active_attr = SIXEL_STATUS_ATTR_NONE;
                    cursor += 3;
                    continue;
                }

                if (cursor[2] == 'B' || cursor[2] == 'E' ||
                    cursor[2] == 'W') {
                    token = cursor[2];
                    start_sequence = NULL;
                    target_attr = SIXEL_STATUS_ATTR_NONE;

                    if (token == 'B') {
                        target_attr = SIXEL_STATUS_ATTR_BOLD;
                        if (use_sequences != 0 && have_bold != 0) {
                            start_sequence = "\x1b[1m";
                        }
                    } else if (token == 'E') {
                        target_attr = SIXEL_STATUS_ATTR_ERROR;
                        if (use_sequences != 0 && have_color != 0) {
                            start_sequence = "\x1b[31m";
                        }
                    } else {
                        target_attr = SIXEL_STATUS_ATTR_WARNING;
                        if (use_sequences != 0 && have_color != 0) {
                            start_sequence = "\x1b[33m";
                        }
                    }

                    if (target_attr != active_attr) {
                        if (sequence_active != 0 && use_sequences != 0) {
                            appended = sixel_status_append_sequence(
                                destination,
                                destination_size,
                                index,
                                "\x1b[0m");
                            index = appended;
                            if (index >= destination_size) {
                                break;
                            }
                            sequence_active = 0;
                        }

                        if (start_sequence != NULL) {
                            appended = sixel_status_append_sequence(
                                destination,
                                destination_size,
                                index,
                                start_sequence);
                            index = appended;
                            if (index >= destination_size) {
                                break;
                            }
                            sequence_active = 1;
                        }

                        active_attr = target_attr;
                    }

                    cursor += 3;
                    continue;
                }

                appended = sixel_status_append_literal(destination,
                                                       destination_size,
                                                       index,
                                                       '\\');
                index = appended;
                if (index >= destination_size) {
                    break;
                }
                cursor += 1;
                continue;
            }
        }

        appended = sixel_status_append_literal(destination,
                                               destination_size,
                                               index,
                                               cursor[0]);
        index = appended;
        if (index >= destination_size) {
            break;
        }
        cursor += 1;
    }

    if (sequence_active != 0 && use_sequences != 0) {
        appended = sixel_status_append_sequence(destination,
                                                destination_size,
                                                index,
                                                "\x1b[0m");
        index = appended;
    }

    if (index >= destination_size) {
        index = destination_size - 1;
        destination[index] = '\0';
    }

    return index;
}

static int
sixel_status_force_colors_enabled(void)
{
    const char *value;

    value = sixel_compat_getenv("SIXEL_STATUS_FORCE_COLORS");
    if (value == NULL) {
        return 0;
    }

    /*
     * Keep the override intentionally strict so test environments can opt in
     * without changing behavior for unrelated values.
     */
    if (strcmp(value, "1") == 0) {
        return 1;
    }

    return 0;
}


/* convert error status code int formatted string */
SIXELAPI char const *
sixel_helper_format_error(
    SIXELSTATUS     /* in */  status           /* status code */
)
{
    static char buffer[1024];
    char const *error_string;

    switch (status & 0x1000) {
    case SIXEL_OK:
        switch (status) {
        case SIXEL_INTERRUPTED:
            error_string = SIXEL_MESSAGE_INTERRUPTED;
            break;
        case SIXEL_OK:
        default:
            error_string = SIXEL_MESSAGE_OK;
            break;
        }
        break;
    case SIXEL_FALSE:
        switch (status & 0x1f00) {
        case SIXEL_RUNTIME_ERROR:
            switch (status) {
            case SIXEL_BAD_ALLOCATION:
                error_string = SIXEL_MESSAGE_BAD_ALLOCATION;
                break;
            case SIXEL_BAD_ARGUMENT:
                error_string = SIXEL_MESSAGE_BAD_ARGUMENT;
                break;
            case SIXEL_BAD_INPUT:
                error_string = SIXEL_MESSAGE_BAD_INPUT;
                break;
            case SIXEL_LOADER_FAILED:
                error_string = SIXEL_MESSAGE_LOADER_FAILED;
                break;
            case SIXEL_BAD_CLIPBOARD:
                error_string = SIXEL_MESSAGE_BAD_CLIPBOARD;
                break;
            case SIXEL_BAD_INTEGER_OVERFLOW:
                error_string = SIXEL_MESSAGE_BAD_INTEGER_OVERFLOW;
                break;
            default:
                error_string = SIXEL_MESSAGE_RUNTIME_ERROR;
                break;
            }
            break;
        case SIXEL_LOGIC_ERROR:
            error_string = SIXEL_MESSAGE_LOGIC_ERROR;
            break;
        case SIXEL_FEATURE_ERROR:
            switch (status) {
            case SIXEL_NOT_IMPLEMENTED:
                error_string = SIXEL_MESSAGE_NOT_IMPLEMENTED;
                break;
            default:
                error_string = SIXEL_MESSAGE_FEATURE_ERROR;
                break;
            }
            break;
        case SIXEL_LIBC_ERROR:
            if (sixel_compat_strerror(errno,
                                      buffer,
                                      sizeof(buffer)) == NULL) {
                buffer[0] = '\0';
            }
            error_string = buffer;
            break;
#ifdef HAVE_LIBCURL
        case SIXEL_CURL_ERROR:
            error_string = curl_easy_strerror(status & 0xff);
            break;
#endif
#ifdef HAVE_JPEG
        case SIXEL_JPEG_ERROR:
            error_string = SIXEL_MESSAGE_JPEG_ERROR;
            break;
#endif
#ifdef HAVE_LIBPNG
        case SIXEL_PNG_ERROR:
            error_string = SIXEL_MESSAGE_PNG_ERROR;
            break;
#endif
#ifdef HAVE_LIBTIFF
        case SIXEL_TIFF_ERROR:
            error_string = SIXEL_MESSAGE_TIFF_ERROR;
            break;
#endif
#ifdef HAVE_GDK_PIXBUF2
        case SIXEL_GDK_ERROR:
            error_string = SIXEL_MESSAGE_GDK_ERROR;
            break;
#endif
#ifdef HAVE_GD
        case SIXEL_GD_ERROR:
            error_string = SIXEL_MESSAGE_GD_ERROR;
            break;
#endif
        case SIXEL_STBI_ERROR:
            error_string = SIXEL_MESSAGE_STBI_ERROR;
            break;
        case SIXEL_STBIW_ERROR:
            error_string = SIXEL_MESSAGE_STBIW_ERROR;
            break;
        case SIXEL_COM_ERROR:
            error_string = SIXEL_MESSAGE_COM_ERROR;
            break;
        case SIXEL_WIC_ERROR:
            error_string = SIXEL_MESSAGE_WIC_ERROR;
            break;
        case SIXEL_FALSE:
            error_string = SIXEL_MESSAGE_FALSE;
            break;
        default:
            error_string = SIXEL_MESSAGE_UNEXPECTED;
            break;
        }
        break;
    default:
        error_string = SIXEL_MESSAGE_UNEXPECTED;
        break;
    }
    return error_string;
}


/* Confirm simple status codes format to well-known strings. */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
