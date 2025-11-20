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
 * Lightweight fences for producer/consumer coordination.
 */
#ifndef LIBSIXEL_SIXEL_ATOMIC_H
#define LIBSIXEL_SIXEL_ATOMIC_H

#if defined(__STDC_NO_ATOMICS__) || !defined(__STDC_VERSION__) || \
    __STDC_VERSION__ < 201112L
typedef unsigned int sixel_atomic_u32_t;

# if defined(__GNUC__)
#  define sixel_fence_release() __atomic_thread_fence(__ATOMIC_RELEASE)
#  define sixel_fence_acquire() __atomic_thread_fence(__ATOMIC_ACQUIRE)
static inline unsigned int
sixel_atomic_fetch_add_u32(sixel_atomic_u32_t *ptr,
                           unsigned int value)
{
    return __atomic_fetch_add(ptr, value, __ATOMIC_ACQ_REL);
}

static inline unsigned int
sixel_atomic_fetch_sub_u32(sixel_atomic_u32_t *ptr,
                           unsigned int value)
{
    return __atomic_fetch_sub(ptr, value, __ATOMIC_ACQ_REL);
}
# elif defined(_MSC_VER)
#  include <windows.h>
#  define sixel_fence_release() MemoryBarrier()
#  define sixel_fence_acquire() MemoryBarrier()
static inline unsigned int
sixel_atomic_fetch_add_u32(sixel_atomic_u32_t *ptr,
                           unsigned int value)
{
    return (unsigned int)InterlockedExchangeAdd(
        (volatile LONG *)ptr, (LONG)value);
}

static inline unsigned int
sixel_atomic_fetch_sub_u32(sixel_atomic_u32_t *ptr,
                           unsigned int value)
{
    return (unsigned int)InterlockedExchangeAdd(
        (volatile LONG *)ptr, -(LONG)value);
}
# else
#  define sixel_fence_release() do { } while (0)
#  define sixel_fence_acquire() do { } while (0)
static inline unsigned int
sixel_atomic_fetch_add_u32(sixel_atomic_u32_t *ptr,
                           unsigned int value)
{
    unsigned int previous;

    previous = *ptr;
    *ptr += value;

    return previous;
}

static inline unsigned int
sixel_atomic_fetch_sub_u32(sixel_atomic_u32_t *ptr,
                           unsigned int value)
{
    unsigned int previous;

    previous = *ptr;
    *ptr -= value;

    return previous;
}
# endif
#else
# include <stdatomic.h>
typedef _Atomic unsigned int sixel_atomic_u32_t;
# define sixel_fence_release() atomic_thread_fence(memory_order_release)
# define sixel_fence_acquire() atomic_thread_fence(memory_order_acquire)
static inline unsigned int
sixel_atomic_fetch_add_u32(sixel_atomic_u32_t *ptr,
                           unsigned int value)
{
    return atomic_fetch_add_explicit(ptr, value, memory_order_acq_rel);
}

static inline unsigned int
sixel_atomic_fetch_sub_u32(sixel_atomic_u32_t *ptr,
                           unsigned int value)
{
    return atomic_fetch_sub_explicit(ptr, value, memory_order_acq_rel);
}
#endif

#endif /* LIBSIXEL_SIXEL_ATOMIC_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
