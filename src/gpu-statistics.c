/* SPDX-License-Identifier: MIT */
/*
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

#include <string.h>

#include "gpu-statistics.h"

/* GPU backends exist only on pthread hosts (Metal on Apple, Vulkan on other
 * POSIX systems), so the static initializer needs no portable once-guard.
 * Builds without a backend keep a constant all-zero snapshot. */
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
#include <pthread.h>

static pthread_mutex_t g_sixel_gpu_statistics_lock =
    PTHREAD_MUTEX_INITIALIZER;
static sixel_gpu_statistics_t g_sixel_gpu_statistics;

static void
sixel_gpu_statistics_copy_name(char *dest, size_t size, char const *name)
{
    size_t length;

    length = name != NULL ? strlen(name) : 0;
    if (length >= size) {
        length = size - 1U;
    }
    if (length != 0) {
        memcpy(dest, name, length);
    }
    dest[length] = '\0';
}

void
sixel_gpu_statistics_set_device(char const *backend, char const *device)
{
    (void)pthread_mutex_lock(&g_sixel_gpu_statistics_lock);
    g_sixel_gpu_statistics.available = 1;
    sixel_gpu_statistics_copy_name(g_sixel_gpu_statistics.backend,
        sizeof(g_sixel_gpu_statistics.backend), backend);
    sixel_gpu_statistics_copy_name(g_sixel_gpu_statistics.device,
        sizeof(g_sixel_gpu_statistics.device), device);
    (void)pthread_mutex_unlock(&g_sixel_gpu_statistics_lock);
}

void
sixel_gpu_statistics_record(unsigned long long busy_nanoseconds)
{
    (void)pthread_mutex_lock(&g_sixel_gpu_statistics_lock);
    ++g_sixel_gpu_statistics.dispatches;
    g_sixel_gpu_statistics.busy_nanoseconds += busy_nanoseconds;
    (void)pthread_mutex_unlock(&g_sixel_gpu_statistics_lock);
}

SIXELAPI SIXELSTATUS
sixel_gpu_get_statistics(sixel_gpu_statistics_t *statistics)
{
    if (statistics == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    (void)pthread_mutex_lock(&g_sixel_gpu_statistics_lock);
    *statistics = g_sixel_gpu_statistics;
    (void)pthread_mutex_unlock(&g_sixel_gpu_statistics_lock);
    return SIXEL_OK;
}

#else

void
sixel_gpu_statistics_set_device(char const *backend, char const *device)
{
    (void)backend;
    (void)device;
}

void
sixel_gpu_statistics_record(unsigned long long busy_nanoseconds)
{
    (void)busy_nanoseconds;
}

SIXELAPI SIXELSTATUS
sixel_gpu_get_statistics(sixel_gpu_statistics_t *statistics)
{
    if (statistics == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(statistics, 0, sizeof(*statistics));
    return SIXEL_OK;
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
