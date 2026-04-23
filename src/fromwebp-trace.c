/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See AUTHORS.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

/* STDC_HEADERS */
#include <stdio.h>

#if HAVE_STRING_H
# include <string.h>
#endif

#include "fromwebp-internal.h"
#include "loader-common.h"

#define SIXEL_WEBP_TRACE_CODE_MAX 32u

#if defined(_MSC_VER)
# define SIXEL_WEBP_TRACE_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !defined(__PCC__)
# define SIXEL_WEBP_TRACE_TLS _Thread_local
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__PCC__)
# define SIXEL_WEBP_TRACE_TLS __thread
#else
# define SIXEL_WEBP_TRACE_TLS
#endif

static SIXEL_WEBP_TRACE_TLS char const *
sixel_webp_trace_codes[SIXEL_WEBP_TRACE_CODE_MAX];
static SIXEL_WEBP_TRACE_TLS unsigned int
sixel_webp_trace_code_count;

static void
sixel_webp_trace_reset(void)
{
    sixel_webp_trace_code_count = 0u;
}

void
sixel_webp_trace_contract_add_code(char const *code)
{
    unsigned int i;

    i = 0u;
    if (code == NULL || code[0] == '\0') {
        return;
    }
    for (i = 0u; i < sixel_webp_trace_code_count; ++i) {
        if (strcmp(sixel_webp_trace_codes[i], code) == 0) {
            return;
        }
    }
    if (sixel_webp_trace_code_count >= SIXEL_WEBP_TRACE_CODE_MAX) {
        return;
    }
    sixel_webp_trace_codes[sixel_webp_trace_code_count] = code;
    ++sixel_webp_trace_code_count;
}

void
sixel_webp_trace_contract_flush(int rc)
{
    unsigned int i;
    char const *kind;

    i = 0u;
    kind = rc == 0 ? "OK" : "ERR";
    if (!sixel_trace_topic_is_enabled("webp_decode")) {
        sixel_webp_trace_reset();
        return;
    }

    if (sixel_webp_trace_code_count == 0u) {
        if (rc == 0) {
            sixel_webp_trace_contract_add_code("WEBP_OK");
        } else {
            sixel_webp_trace_contract_add_code("WEBP_ERR");
        }
    }

    /* Keep this machine-readable header stable for shell TAP contracts. */
    fprintf(stderr, "LSXWEBP1|rc=%d|kind=%s|codes=", rc, kind);
    for (i = 0u; i < sixel_webp_trace_code_count; ++i) {
        if (i != 0u) {
            fprintf(stderr, ",");
        }
        fprintf(stderr, "%s", sixel_webp_trace_codes[i]);
    }
    fprintf(stderr, "\n");

    sixel_webp_trace_reset();
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
