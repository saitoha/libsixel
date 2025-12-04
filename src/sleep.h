/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_SLEEP_H
#define LIBSIXEL_SLEEP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sixel_sleep
 *
 * Sleep for the requested number of microseconds.  The helper hides
 * platform specific entry points so callers can request short delays
 * without scattering conditional compilation blocks across the codebase.
 */
void sixel_sleep(unsigned int usec);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_SLEEP_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
