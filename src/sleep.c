/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#if HAVE_CONFIG_H
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
#if HAVE_NANOSLEEP
    struct timespec request;
    time_t seconds;
    long nanoseconds;

    seconds = (time_t)(usec / 1000000u);
    nanoseconds = (long)((usec % 1000000u) * 1000u);
    request.tv_sec = seconds;
    request.tv_nsec = nanoseconds;

    (void)nanosleep(&request, NULL);
#elif defined(_WIN32)
    DWORD millis;

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
#else
    (void)usleep(usec);
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
