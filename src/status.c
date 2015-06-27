/*
 * Copyright (c) 2014,2015 Hayaki Saito
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

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include <sixel.h>

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif
#ifdef HAVE_LIBCURL
# include <curl/curl.h>
#endif

#include "status.h"

#define SIXEL_MESSAGE_OK                ("succeeded")
#define SIXEL_MESSAGE_INTERRUPTED       ("interrupted by a signal")
#define SIXEL_MESSAGE_BAD_ALLOCATION    ("runtime error: bad allocation error")
#define SIXEL_MESSAGE_BAD_ARGUMENT      ("runtime error: bad argument detected")
#define SIXEL_MESSAGE_BAD_INPUT         ("runtime error: bad input detected")
#define SIXEL_MESSAGE_RUNTIME_ERROR     ("runtime error")
#define SIXEL_MESSAGE_LOGIC_ERROR       ("logic error")
#define SIXEL_MESSAGE_NOT_IMPLEMENTED   ("feature error: not implemented")
#define SIXEL_MESSAGE_FEATURE_ERROR     ("feature error")

static char g_buffer[1024] = { 0x0 };

SIXELAPI void
sixel_helper_set_additional_message(
    const char      /* in */  *message         /* error message */
)
{
    size_t len;

    len = strlen(message);
    memcpy(g_buffer, message, len < sizeof(g_buffer) ? len: sizeof(g_buffer) - 1);
    g_buffer[sizeof(g_buffer) - 1] = 0;
}


SIXELAPI char const *
sixel_helper_get_additional_message(void)
{
    return g_buffer;
}


/* convert error status code int formatted string */
SIXELAPI char const *
sixel_helper_format_error(
    SIXELSTATUS     /* in */  status           /* status code */
)
{
    static char buffer[1024];
    char const *error_string;
    char *p;
    size_t len;

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
            p = strerror(errno);
            len = strlen(p) + 1;
            memcpy(buffer, p, len < sizeof(buffer) ? len: sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = 0;
            free((char *)p);
            error_string = buffer;
            break;
#ifdef HAVE_LIBCURL
        case SIXEL_CURL_ERROR:
            error_string = curl_easy_strerror(status & 0xff);
            break;
#endif
        case SIXEL_JPEG_ERROR:
            error_string = "jpeg error";
            break;
        case SIXEL_PNG_ERROR:
            error_string = "png error";
            break;
        case SIXEL_GDK_ERROR:
            error_string = "gdk error";
            break;
        case SIXEL_GD_ERROR:
            error_string = "gd error";
            break;
        case SIXEL_STBI_ERROR:
            error_string = "stb_image error";
            break;
        case SIXEL_STBIW_ERROR:
            error_string = "stb_image_write error";
            break;
        case SIXEL_FALSE:
            error_string = "unknown error (SIXEL_FALSE)";
            break;
        default:
            error_string = "unknown error";
            break;
        }
        break;
    default:
        error_string = "unknown error";
        break;
    }
    return error_string;
}


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;

    if (strcmp(sixel_helper_format_error(SIXEL_OK), SIXEL_MESSAGE_OK) != 0) {
        goto error;
    }
    if (strcmp(sixel_helper_format_error(SIXEL_INTERRUPTED), SIXEL_MESSAGE_INTERRUPTED) != 0) {
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
    if (strcmp(sixel_helper_format_error(SIXEL_BAD_ALLOCATION), SIXEL_MESSAGE_BAD_ALLOCATION) != 0) {
        goto error;
    }
    if (strcmp(sixel_helper_format_error(SIXEL_BAD_ARGUMENT), SIXEL_MESSAGE_BAD_ARGUMENT) != 0) {
        goto error;
    }
    if (strcmp(sixel_helper_format_error(SIXEL_BAD_INPUT), SIXEL_MESSAGE_BAD_INPUT) != 0) {
        goto error;
    }
    if (strcmp(sixel_helper_format_error(SIXEL_RUNTIME_ERROR), SIXEL_MESSAGE_RUNTIME_ERROR) != 0) {
        goto error;
    }
    if (strcmp(sixel_helper_format_error(SIXEL_LOGIC_ERROR), SIXEL_MESSAGE_LOGIC_ERROR) != 0) {
        goto error;
    }
    if (strcmp(sixel_helper_format_error(SIXEL_NOT_IMPLEMENTED), SIXEL_MESSAGE_NOT_IMPLEMENTED) != 0) {
        goto error;
    }
    if (strcmp(sixel_helper_format_error(SIXEL_FEATURE_ERROR), SIXEL_MESSAGE_FEATURE_ERROR) != 0) {
        goto error;
    }

    return EXIT_SUCCESS;
error:
    perror("test1");
    return nret;
}


int
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

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
