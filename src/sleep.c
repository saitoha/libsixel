/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

/* STDC_HEADERS */
#include <stddef.h>

#if HAVE_TIME_H
# include <time.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if defined(_WIN32)
# include <windows.h>
#endif

#include "sleep.h"

void
sixel_sleep(unsigned int usec)
{
#if defined(HAVE_CLOCK) && !HAVE_NANOSLEEP && \
    !defined(_WIN32) && !defined(HAVE_USLEEP)
    clock_t start;
    clock_t elapsed;
    clock_t target_ticks;
#endif
#if HAVE_NANOSLEEP
    struct timespec request;
    time_t seconds;
    long nanoseconds;
#elif defined(_WIN32)
    DWORD millis;
#endif

#if HAVE_NANOSLEEP
    seconds = (time_t)(usec / 1000000u);
    nanoseconds = (long)((usec % 1000000u) * 1000u);
    request.tv_sec = seconds;
    request.tv_nsec = nanoseconds;

    (void)nanosleep(&request, NULL);
#elif defined(_WIN32)
    /*
     * Round up to the nearest millisecond because the Windows Sleep API
     * accepts millisecond values.  A zero microsecond request still waits
     * for one tick so callers avoid busy waiting.
     */
    millis = (DWORD)((usec + 999u) / 1000u);
    if (millis == 0) {
        millis = 1;
    }
    Sleep(millis);
#elif defined(HAVE_USLEEP)
    (void)usleep(usec);
#elif defined(HAVE_CLOCK)
    /*
     * Emscripten does not expose usleep(), so fall back to clock().
     * We spin on process CPU time using integer math to avoid
     * floating point, with the following steps:
     *
     * - Convert microseconds to clock ticks with rounding up.
     * - Capture the current clock value as the starting point.
     * - Loop until the elapsed ticks reach the target delay.
     */
    target_ticks = (clock_t)((((unsigned long long)usec *
                               (unsigned long long)CLOCKS_PER_SEC) +
                              999999u) / 1000000u);
    if (target_ticks <= 0) {
        return;
    }

    start = clock();
    if (start == (clock_t)-1) {
        return;
    }

    elapsed = 0;
    while (elapsed < target_ticks) {
        elapsed = clock() - start;
        if (elapsed < 0) {
            break;
        }
    }
#else
    /*
     * No portable sleep primitive was detected for this toolchain.
     * Keep the helper as a no-op and mark the argument as used so
     * -Werror/-Wunused-parameter builds (for example emscripten) pass.
     */
    (void)usec;
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
