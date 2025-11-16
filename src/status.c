/*
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

#include "config.h"

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

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
#define SIXEL_MESSAGE_RUNTIME_ERROR         ("runtime error")
#define SIXEL_MESSAGE_LOGIC_ERROR           ("logic error")
#define SIXEL_MESSAGE_NOT_IMPLEMENTED       ("feature error: not implemented")
#define SIXEL_MESSAGE_FEATURE_ERROR         ("feature error")
#define SIXEL_MESSAGE_STBI_ERROR            ("stb_image error")
#define SIXEL_MESSAGE_STBIW_ERROR           ("stb_image_write error")
#define SIXEL_MESSAGE_JPEG_ERROR            ("libjpeg error")
#define SIXEL_MESSAGE_PNG_ERROR             ("libpng error")
#define SIXEL_MESSAGE_GDK_ERROR             ("GDK error")
#define SIXEL_MESSAGE_GD_ERROR              ("GD error")

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
    SIXEL_STATUS_MESSAGE_LIMIT = 4096
};

static char status_markup_buffer[SIXEL_STATUS_MESSAGE_LIMIT] = { 0x0 };
static char status_render_buffer[SIXEL_STATUS_MESSAGE_LIMIT] = { 0x0 };

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

/*
 * Markup overview
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

/* set detailed error message (thread-unsafe) */
SIXELAPI void
sixel_helper_set_additional_message(
    const char      /* in */  *message         /* error message */
)
{
    size_t length;
    size_t copy_length;

    if (message == NULL) {
        status_markup_buffer[0] = '\0';
        status_render_buffer[0] = '\0';
        return;
    }

    length = strlen(message);
    copy_length = length;

    if (copy_length >= SIXEL_STATUS_MESSAGE_LIMIT) {
        copy_length = SIXEL_STATUS_MESSAGE_LIMIT - 1;
        copy_length = sixel_status_utf8_trim_length(message,
                                                    copy_length);
    }

    memcpy(status_markup_buffer, message, copy_length);
    status_markup_buffer[copy_length] = '\0';
    status_render_buffer[0] = '\0';
}


/* get detailed error message (thread-unsafe) */
SIXELAPI char const *
sixel_helper_get_additional_message(void)
{
    (void)sixel_status_render_markup(status_markup_buffer,
                                     status_render_buffer,
                                     sizeof(status_render_buffer));
    return status_render_buffer;
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

    index = 0;

    if (destination_size == 0) {
        return 0;
    }

    destination[0] = '\0';
    cursor = source;
    use_sequences = 0;
    have_bold = 0;
    have_color = 0;

#if SIXEL_STATUS_HAVE_TTY_HELPERS
    {
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


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;
    char const *message;

    message = sixel_helper_format_error(SIXEL_OK);
    if (strcmp(message, SIXEL_MESSAGE_OK) != 0) {
        goto error;
    }

    message = sixel_helper_format_error(SIXEL_INTERRUPTED);
    if (strcmp(message, SIXEL_MESSAGE_INTERRUPTED) != 0) {
        goto error;
    }
    return EXIT_SUCCESS;
error:
    perror("test1");
    return nret;
}


static int
test2(void)
{
    int nret = EXIT_FAILURE;
    char const *message;

    message = sixel_helper_format_error(SIXEL_BAD_ALLOCATION);
    if (strcmp(message, SIXEL_MESSAGE_BAD_ALLOCATION) != 0) {
        goto error;
    }

    message = sixel_helper_format_error(SIXEL_BAD_ARGUMENT);
    if (strcmp(message, SIXEL_MESSAGE_BAD_ARGUMENT) != 0) {
        goto error;
    }

    message = sixel_helper_format_error(SIXEL_BAD_INPUT);
    if (strcmp(message, SIXEL_MESSAGE_BAD_INPUT) != 0) {
        goto error;
    }

    message = sixel_helper_format_error(SIXEL_RUNTIME_ERROR);
    if (strcmp(message, SIXEL_MESSAGE_RUNTIME_ERROR) != 0) {
        goto error;
    }

    message = sixel_helper_format_error(SIXEL_LOGIC_ERROR);
    if (strcmp(message, SIXEL_MESSAGE_LOGIC_ERROR) != 0) {
        goto error;
    }

    message = sixel_helper_format_error(SIXEL_NOT_IMPLEMENTED);
    if (strcmp(message, SIXEL_MESSAGE_NOT_IMPLEMENTED) != 0) {
        goto error;
    }

    message = sixel_helper_format_error(SIXEL_FEATURE_ERROR);
    if (strcmp(message, SIXEL_MESSAGE_FEATURE_ERROR) != 0) {
        goto error;
    }

    message = sixel_helper_format_error(SIXEL_LIBC_ERROR);
    if (strcmp(message, SIXEL_MESSAGE_UNEXPECTED) == 0) {
        goto error;
    }

#ifdef HAVE_LIBCURL
    message = sixel_helper_format_error(SIXEL_CURL_ERROR);
    if (strcmp(message, SIXEL_MESSAGE_UNEXPECTED) == 0) {
        goto error;
    }
#endif

#if HAVE_JPEG
    message = sixel_helper_format_error(SIXEL_JPEG_ERROR);
    if (strcmp(message, SIXEL_MESSAGE_JPEG_ERROR) != 0) {
        goto error;
    }
#endif

#if HAVE_LIBPNG
    message = sixel_helper_format_error(SIXEL_PNG_ERROR);
    if (strcmp(message, SIXEL_MESSAGE_PNG_ERROR) != 0) {
        goto error;
    }
#endif

#if HAVE_GD
    message = sixel_helper_format_error(SIXEL_GD_ERROR);
    if (strcmp(message, SIXEL_MESSAGE_GD_ERROR) != 0) {
        goto error;
    }
#endif

#if HAVE_GDK_PIXBUF2
    message = sixel_helper_format_error(SIXEL_GDK_ERROR);
    if (strcmp(message, SIXEL_MESSAGE_GDK_ERROR) != 0) {
        goto error;
    }
#endif

    message = sixel_helper_format_error(SIXEL_STBI_ERROR);
    if (strcmp(message, SIXEL_MESSAGE_STBI_ERROR) != 0) {
        goto error;
    }

    message = sixel_helper_format_error(SIXEL_STBIW_ERROR);
    if (strcmp(message, SIXEL_MESSAGE_STBIW_ERROR) != 0) {
        goto error;
    }

    return EXIT_SUCCESS;
error:
    perror("test2");
    return nret;
}


SIXELAPI int
sixel_status_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
        test2,
    };

    for (i = 0; i < sizeof(testcases) / sizeof(testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
