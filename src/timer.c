/*
 * SPDX-License-Identifier: MIT
 *
 * High-resolution timer helpers used across libsixel.
 * This translation unit isolates the wall clock probe so that
 * lightweight frontends (Quick Look, WIC) can link the logger
 * without dragging in the full assessment pipeline or ONNX
 * dependencies.
 */

#include "config.h"

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

    if (sixel_compat_gettimeofday(&tv, NULL) != 0) {
        return 0.0;
    }
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#else
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}

