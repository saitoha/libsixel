/*
 * Lightweight fences for producer/consumer coordination.
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */
#ifndef LIBSIXEL_SIXEL_ATOMIC_H
#define LIBSIXEL_SIXEL_ATOMIC_H

#if defined(__STDC_NO_ATOMICS__)
# if defined(__GNUC__)
#  define sixel_fence_release() __atomic_thread_fence(__ATOMIC_RELEASE)
#  define sixel_fence_acquire() __atomic_thread_fence(__ATOMIC_ACQUIRE)
# elif defined(_MSC_VER)
#  include <windows.h>
#  define sixel_fence_release() MemoryBarrier()
#  define sixel_fence_acquire() MemoryBarrier()
# else
#  define sixel_fence_release() do { } while (0)
#  define sixel_fence_acquire() do { } while (0)
# endif
#else
# include <stdatomic.h>
# define sixel_fence_release() atomic_thread_fence(memory_order_release)
# define sixel_fence_acquire() atomic_thread_fence(memory_order_acquire)
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
