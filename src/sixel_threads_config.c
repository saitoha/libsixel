/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>

#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */

#include <sixel.h>
#include "sixel_threads_config.h"
#if SIXEL_ENABLE_THREADS
# include "sixel_threading.h"
#endif

/*
 * Thread configuration keeps the precedence rules centralized:
 *   1. Library callers may override via `sixel_set_threads`.
 *   2. Otherwise, `SIXEL_THREADS` from the environment is honored.
 *   3. Fallback defaults to single threaded execution.
 */
typedef struct sixel_thread_config_state {
    int requested_threads;
    int override_active;
    int env_threads;
    int env_valid;
    int env_checked;
} sixel_thread_config_state_t;

static sixel_thread_config_state_t g_thread_config = {
    1,
    0,
    1,
    0,
    0
};

static int
sixel_threads_token_is_auto(char const *text)
{
    if (text == NULL) {
        return 0;
    }

    if ((text[0] == 'a' || text[0] == 'A') &&
        (text[1] == 'u' || text[1] == 'U') &&
        (text[2] == 't' || text[2] == 'T') &&
        (text[3] == 'o' || text[3] == 'O') &&
        text[4] == '\0') {
        return 1;
    }

    return 0;
}

SIXELAPI int
sixel_threads_normalize(int requested)
{
    int normalized;

#if SIXEL_ENABLE_THREADS
    int hw_threads;

    if (requested <= 0) {
        hw_threads = sixel_get_hw_threads();
        if (hw_threads < 1) {
            hw_threads = 1;
        }
        normalized = hw_threads;
    } else {
        normalized = requested;
    }

    if (normalized < 1) {
        normalized = 1;
    }
#else
    (void)requested;
    normalized = 1;
#endif

    return normalized;
}

static int
sixel_threads_parse_env_value(char const *text, int *value)
{
    long parsed;
    char *endptr;
    int normalized;

    if (text == NULL || value == NULL) {
        return 0;
    }

    if (sixel_threads_token_is_auto(text)) {
        normalized = sixel_threads_normalize(0);
        *value = normalized;
        return 1;
    }

    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0' || errno == ERANGE) {
        return 0;
    }

    if (parsed < 1) {
        normalized = sixel_threads_normalize(1);
    } else if (parsed > INT_MAX) {
        normalized = sixel_threads_normalize(INT_MAX);
    } else {
        normalized = sixel_threads_normalize((int)parsed);
    }

    *value = normalized;
    return 1;
}

static void
sixel_threads_load_env(void)
{
    char const *text;
    int parsed;

    if (g_thread_config.env_checked) {
        return;
    }

    g_thread_config.env_checked = 1;
    g_thread_config.env_valid = 0;

    text = getenv("SIXEL_THREADS");
    if (text == NULL || text[0] == '\0') {
        return;
    }

    if (sixel_threads_parse_env_value(text, &parsed)) {
        g_thread_config.env_threads = parsed;
        g_thread_config.env_valid = 1;
    }
}

SIXELAPI int
sixel_threads_resolve(void)
{
    int resolved;

#if SIXEL_ENABLE_THREADS
    if (g_thread_config.override_active) {
        return g_thread_config.requested_threads;
    }
#endif

    sixel_threads_load_env();

#if SIXEL_ENABLE_THREADS
    if (g_thread_config.env_valid) {
        resolved = g_thread_config.env_threads;
    } else {
        resolved = sixel_threads_normalize(0);
    }
#else
    resolved = 1;
#endif

    return resolved;
}

/*
 * Public setter so CLI/bindings may override the runtime thread preference.
 */
SIXELAPI void
sixel_set_threads(int threads)
{
#if SIXEL_ENABLE_THREADS
    g_thread_config.requested_threads = sixel_threads_normalize(threads);
#else
    (void)threads;
    g_thread_config.requested_threads = 1;
#endif
    g_thread_config.override_active = 1;
}

