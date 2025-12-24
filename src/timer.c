/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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
/*
 * High-resolution timer helpers used across libsixel.
 * This translation unit isolates the wall clock probe so that
 * lightweight frontends (Quick Look, WIC) can link the logger
 * without dragging in the full assessment pipeline or ONNX
 * dependencies.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "timer.h"
#include "compat_stub.h"

#include <sixel.h>

#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

SIXELAPI double
sixel_assessment_timer_now(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    BOOL ok;

    if (frequency.QuadPart == 0) {
        ok = QueryPerformanceFrequency(&frequency);
        if (!ok || frequency.QuadPart == 0) {
            return (double)GetTickCount64() / 1000.0;
        }
    }
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#elif defined(HAVE_CLOCK_GETTIME)
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#elif defined(HAVE_GETTIMEOFDAY)
    struct timeval tv;

    if (sixel_compat_gettimeofday(&tv) != 0) {
        return 0.0;
    }
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#else
    return (double)clock() / (double)CLOCKS_PER_SEC;
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
