/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LIBSIXEL_THREADPOOL_H
#define LIBSIXEL_THREADPOOL_H

#include <stddef.h>

#include <sixel.h>

#include "sixel_threading.h"

typedef struct {
    int band_index;
} tp_job_t;

typedef int (*tp_worker_fn)(tp_job_t job, void *userdata, void *workspace);

struct threadpool;
typedef struct threadpool threadpool_t;

SIXELAPI threadpool_t *threadpool_create(int nthreads,
                                         int qsize,
                                         size_t workspace_size,
                                         tp_worker_fn worker,
                                         void *userdata);
SIXELAPI void threadpool_destroy(threadpool_t *pool);
SIXELAPI void threadpool_push(threadpool_t *pool, tp_job_t job);
SIXELAPI void threadpool_finish(threadpool_t *pool);
SIXELAPI int threadpool_get_error(threadpool_t *pool);
SIXELAPI int threadpool_grow(threadpool_t *pool, int additional_threads);

#endif /* LIBSIXEL_THREADPOOL_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
