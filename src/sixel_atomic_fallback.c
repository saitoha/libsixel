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
# include "config.h"
#endif

#include <stdlib.h>

#include "sixel_atomic.h"

#if (defined(__STDC_NO_ATOMICS__) || !defined(__STDC_VERSION__) || \
     __STDC_VERSION__ < 201112L) && \
    !(defined(__GNUC__) && defined(__ATOMIC_ACQ_REL)) && \
    !defined(_MSC_VER) && \
    defined(SIXEL_ENABLE_THREADS) && SIXEL_ENABLE_THREADS

# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
     && !defined(WITH_WINPTHREAD)
#  if !defined(UNICODE)
#   define UNICODE
#  endif
#  if !defined(_UNICODE)
#   define _UNICODE
#  endif
#  if !defined(WIN32_LEAN_AND_MEAN)
#   define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>

static CRITICAL_SECTION sixel_atomic_fallback_mutex;
static INIT_ONCE sixel_atomic_fallback_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
sixel_atomic_fallback_init_once(PINIT_ONCE once,
                                PVOID parameter,
                                PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;

    InitializeCriticalSection(&sixel_atomic_fallback_mutex);
    return TRUE;
}

static void
sixel_atomic_fallback_lock(void)
{
    BOOL initialized;

    initialized = InitOnceExecuteOnce(&sixel_atomic_fallback_once,
                                      sixel_atomic_fallback_init_once,
                                      NULL,
                                      NULL);
    if (!initialized) {
        abort();
    }
    EnterCriticalSection(&sixel_atomic_fallback_mutex);
}

static void
sixel_atomic_fallback_unlock(void)
{
    LeaveCriticalSection(&sixel_atomic_fallback_mutex);
}

# else
#  include <pthread.h>

/*
 * Use runtime initialization so Cosmopolitan/pcc builds avoid warnings from
 * partial pthread struct initializers.
 */
static pthread_mutex_t sixel_atomic_fallback_mutex;
static pthread_once_t sixel_atomic_fallback_mutex_once = PTHREAD_ONCE_INIT;
static int sixel_atomic_fallback_mutex_ready = 0;

static void
sixel_atomic_fallback_init_once(void)
{
    int rc;

    rc = pthread_mutex_init(&sixel_atomic_fallback_mutex, NULL);
    if (rc == 0) {
        sixel_atomic_fallback_mutex_ready = 1;
    }
}

static void
sixel_atomic_fallback_lock(void)
{
    int rc;
    int once_status;

    once_status = pthread_once(&sixel_atomic_fallback_mutex_once,
                               sixel_atomic_fallback_init_once);
    if (once_status != 0 || !sixel_atomic_fallback_mutex_ready) {
        abort();
    }

    rc = pthread_mutex_lock(&sixel_atomic_fallback_mutex);
    if (rc != 0) {
        abort();
    }
}

static void
sixel_atomic_fallback_unlock(void)
{
    int rc;

    if (!sixel_atomic_fallback_mutex_ready) {
        abort();
    }
    rc = pthread_mutex_unlock(&sixel_atomic_fallback_mutex);
    if (rc != 0) {
        abort();
    }
}
# endif

void
sixel_atomic_fallback_fence_release(void)
{
    /*
     * Reuse the fallback mutex to publish preceding writes when the compiler
     * lacks native atomic fences.
     */
    sixel_atomic_fallback_lock();
    sixel_atomic_fallback_unlock();
}

void
sixel_atomic_fallback_fence_acquire(void)
{
    /*
     * Pair with the release helper above so readers can observe data guarded
     * by fallback atomic flags on weak memory-order architectures.
     */
    sixel_atomic_fallback_lock();
    sixel_atomic_fallback_unlock();
}

unsigned int
sixel_atomic_fallback_fetch_add_u32(sixel_atomic_u32_t *ptr,
                                    unsigned int value)
{
    unsigned int previous;

    previous = 0u;
    sixel_atomic_fallback_lock();
    previous = *ptr;
    *ptr += value;
    sixel_atomic_fallback_unlock();

    return previous;
}

unsigned int
sixel_atomic_fallback_fetch_sub_u32(sixel_atomic_u32_t *ptr,
                                    unsigned int value)
{
    unsigned int previous;

    previous = 0u;
    sixel_atomic_fallback_lock();
    previous = *ptr;
    *ptr -= value;
    sixel_atomic_fallback_unlock();

    return previous;
}

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
