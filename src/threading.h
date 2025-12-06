/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LIBSIXEL_THREADING_H
#define LIBSIXEL_THREADING_H

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*sixel_thread_fn)(void *arg);

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
#include <windows.h>

struct sixel_mutex {
    CRITICAL_SECTION native;
};

struct sixel_cond {
    CONDITION_VARIABLE native;
};

struct sixel_thread {
    HANDLE handle;         /* OS thread handle */
    sixel_thread_fn fn;    /* user callback */
    void *arg;             /* user data */
    int result;            /* callback result */
    int started;           /* thread start flag */
};
#else
#include <pthread.h>

struct sixel_mutex {
    pthread_mutex_t native;
};

struct sixel_cond {
    pthread_cond_t native;
};

struct sixel_thread {
    pthread_t handle;
    sixel_thread_fn fn;
    void *arg;
    int result;
    int started;
};
#endif

typedef struct sixel_mutex sixel_mutex_t;
typedef struct sixel_cond sixel_cond_t;
typedef struct sixel_thread sixel_thread_t;

SIXELAPI int sixel_mutex_init(sixel_mutex_t *mutex);
SIXELAPI void sixel_mutex_destroy(sixel_mutex_t *mutex);
SIXELAPI void sixel_mutex_lock(sixel_mutex_t *mutex);
SIXELAPI void sixel_mutex_unlock(sixel_mutex_t *mutex);

SIXELAPI int sixel_cond_init(sixel_cond_t *cond);
SIXELAPI void sixel_cond_destroy(sixel_cond_t *cond);
SIXELAPI void sixel_cond_wait(sixel_cond_t *cond, sixel_mutex_t *mutex);
SIXELAPI void sixel_cond_signal(sixel_cond_t *cond);
SIXELAPI void sixel_cond_broadcast(sixel_cond_t *cond);

SIXELAPI int sixel_thread_create(sixel_thread_t *thread,
                                 sixel_thread_fn fn,
                                 void *arg);
SIXELAPI void sixel_thread_join(sixel_thread_t *thread);
SIXELAPI int sixel_get_hw_threads(void);

SIXELAPI int sixel_threads_normalize(int requested);
SIXELAPI void sixel_set_threads(int threads);
SIXELAPI int sixel_threads_resolve(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_THREADING_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
