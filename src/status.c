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
            error_string = "interrupted by a signal";
            break;
        case SIXEL_OK:
        default:
            error_string = "succeeded";
            break;
        }
        break;
    case SIXEL_FALSE:
        switch (status & 0x1f00) {
        case SIXEL_RUNTIME_ERROR:
            switch (status) {
            case SIXEL_BAD_ALLOCATION:
                error_string = "runtime error: bad allocation error";
                break;
            case SIXEL_BAD_ARGUMENT:
                error_string = "runtime error: bad argument detected";
                break;
            case SIXEL_BAD_INPUT:
                error_string = "runtime error: bad input detected";
                break;
            default:
                error_string = "runtime error";
                break;
            }
            break;
        case SIXEL_LOGIC_ERROR:
            error_string = "logic error";
            break;
        case SIXEL_FEATURE_ERROR:
            switch (status) {
            case SIXEL_NOT_IMPLEMENTED:
                error_string = "feature error: not implemented";
                break;
            default:
                error_string = "feature error";
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

    if (strcmp(sixel_helper_format_error(SIXEL_OK), "succeeded") == 0) {
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
