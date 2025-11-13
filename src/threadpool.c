/*
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

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "threadpool.h"

typedef struct threadpool_worker threadpool_worker_t;

struct threadpool_worker {
    threadpool_t *pool;
    sixel_thread_t thread;
    void *workspace;
    int started;
};

struct threadpool {
    int nthreads;
    int qsize;
    size_t workspace_size;
    tp_worker_fn worker;
    void *userdata;
    tp_job_t *jobs;
    int head;
    int tail;
    int count;
    int running;
    int shutting_down;
    int joined;
    int error;
    int threads_started;
    sixel_mutex_t mutex;
    sixel_cond_t cond_not_empty;
    sixel_cond_t cond_not_full;
    sixel_cond_t cond_drained;
    int mutex_ready;
    int cond_not_empty_ready;
    int cond_not_full_ready;
    int cond_drained_ready;
    threadpool_worker_t *workers;
};

static void threadpool_free(threadpool_t *pool);
static int threadpool_worker_main(void *arg);

/*
 * Release every dynamically allocated component of the pool. Callers must
 * ensure that worker threads have already terminated before invoking this
 * helper; otherwise joining would operate on freed memory.
 */
static void
threadpool_free(threadpool_t *pool)
{
    int i;

    if (pool == NULL) {
        return;
    }
    if (pool->workers != NULL) {
        for (i = 0; i < pool->nthreads; ++i) {
            if (pool->workers[i].workspace != NULL) {
                free(pool->workers[i].workspace);
            }
        }
        free(pool->workers);
    }
    if (pool->jobs != NULL) {
        free(pool->jobs);
    }
    if (pool->cond_drained_ready) {
        sixel_cond_destroy(&pool->cond_drained);
    }
    if (pool->cond_not_full_ready) {
        sixel_cond_destroy(&pool->cond_not_full);
    }
    if (pool->cond_not_empty_ready) {
        sixel_cond_destroy(&pool->cond_not_empty);
    }
    if (pool->mutex_ready) {
        sixel_mutex_destroy(&pool->mutex);
    }
    free(pool);
}

/*
 * Worker threads pull jobs from the ring buffer, execute the supplied callback
 * outside the critical section, and record the first failure code. All
 * synchronization is delegated to the mutex/condition helpers provided by the
 * threading abstraction.
 */
static int
threadpool_worker_main(void *arg)
{
    threadpool_worker_t *worker;
    threadpool_t *pool;
    tp_job_t job;
    int rc;

    worker = (threadpool_worker_t *)arg;
    pool = worker->pool;
    for (;;) {
        sixel_mutex_lock(&pool->mutex);
        while (pool->count == 0 && !pool->shutting_down) {
            sixel_cond_wait(&pool->cond_not_empty, &pool->mutex);
        }
        if (pool->count == 0 && pool->shutting_down) {
            sixel_mutex_unlock(&pool->mutex);
            break;
        }
        job = pool->jobs[pool->head];
        pool->head = (pool->head + 1) % pool->qsize;
        pool->count -= 1;
        pool->running += 1;
        sixel_cond_signal(&pool->cond_not_full);
        sixel_mutex_unlock(&pool->mutex);

        rc = pool->worker(job, pool->userdata, worker->workspace);

        sixel_mutex_lock(&pool->mutex);
        pool->running -= 1;
        if (rc != SIXEL_OK && pool->error == SIXEL_OK) {
            pool->error = rc;
        }
        if (pool->count == 0 && pool->running == 0) {
            sixel_cond_broadcast(&pool->cond_drained);
        }
        sixel_mutex_unlock(&pool->mutex);
    }
    return SIXEL_OK;
}

SIXELAPI threadpool_t *
threadpool_create(int nthreads,
                  int qsize,
                  size_t workspace_size,
                  tp_worker_fn worker,
                  void *userdata)
{
    threadpool_t *pool;
    int i;
    int rc;

    if (nthreads <= 0 || qsize <= 0 || worker == NULL) {
        return NULL;
    }
    pool = (threadpool_t *)calloc(1, sizeof(threadpool_t));
    if (pool == NULL) {
        return NULL;
    }
    pool->nthreads = nthreads;
    pool->qsize = qsize;
    pool->workspace_size = workspace_size;
    pool->worker = worker;
    pool->userdata = userdata;
    pool->jobs = NULL;
    pool->head = 0;
    pool->tail = 0;
    pool->count = 0;
    pool->running = 0;
    pool->shutting_down = 0;
    pool->joined = 0;
    pool->error = SIXEL_OK;
    pool->threads_started = 0;
    pool->mutex_ready = 0;
    pool->cond_not_empty_ready = 0;
    pool->cond_not_full_ready = 0;
    pool->cond_drained_ready = 0;
    pool->workers = NULL;

    rc = sixel_mutex_init(&pool->mutex);
    if (rc != SIXEL_OK) {
        errno = EINVAL;
        threadpool_free(pool);
        return NULL;
    }
    pool->mutex_ready = 1;

    rc = sixel_cond_init(&pool->cond_not_empty);
    if (rc != SIXEL_OK) {
        errno = EINVAL;
        threadpool_free(pool);
        return NULL;
    }
    pool->cond_not_empty_ready = 1;

    rc = sixel_cond_init(&pool->cond_not_full);
    if (rc != SIXEL_OK) {
        errno = EINVAL;
        threadpool_free(pool);
        return NULL;
    }
    pool->cond_not_full_ready = 1;

    rc = sixel_cond_init(&pool->cond_drained);
    if (rc != SIXEL_OK) {
        errno = EINVAL;
        threadpool_free(pool);
        return NULL;
    }
    pool->cond_drained_ready = 1;

    pool->jobs = (tp_job_t *)malloc(sizeof(tp_job_t) * (size_t)qsize);
    if (pool->jobs == NULL) {
        threadpool_free(pool);
        return NULL;
    }

    pool->workers = (threadpool_worker_t *)
        calloc((size_t)nthreads, sizeof(threadpool_worker_t));
    if (pool->workers == NULL) {
        threadpool_free(pool);
        return NULL;
    }

    for (i = 0; i < nthreads; ++i) {
        pool->workers[i].pool = pool;
        pool->workers[i].workspace = NULL;
        pool->workers[i].started = 0;
        if (workspace_size > 0) {
            /*
             * Zero-initialize the per-thread workspace so that structures like
             * `sixel_parallel_worker_state_t` start with predictable values.
             * The worker initialization logic assumes fields such as
             * `initialized` are cleared before the first job.
             */
            pool->workers[i].workspace = calloc(1, workspace_size);
            if (pool->workers[i].workspace == NULL) {
                pool->shutting_down = 1;
                sixel_cond_broadcast(&pool->cond_not_empty);
                break;
            }
        }
        rc = sixel_thread_create(&pool->workers[i].thread,
                                 threadpool_worker_main,
                                 &pool->workers[i]);
        if (rc != SIXEL_OK) {
            pool->shutting_down = 1;
            sixel_cond_broadcast(&pool->cond_not_empty);
            break;
        }
        pool->workers[i].started = 1;
        pool->threads_started += 1;
    }

    if (pool->threads_started != nthreads) {
        int started;

        started = pool->threads_started;
        for (i = 0; i < started; ++i) {
            sixel_cond_broadcast(&pool->cond_not_empty);
            sixel_thread_join(&pool->workers[i].thread);
        }
        threadpool_free(pool);
        return NULL;
    }

    return pool;
}

SIXELAPI void
threadpool_destroy(threadpool_t *pool)
{
    if (pool == NULL) {
        return;
    }
    threadpool_finish(pool);
    threadpool_free(pool);
}

SIXELAPI void
threadpool_push(threadpool_t *pool, tp_job_t job)
{
    if (pool == NULL) {
        return;
    }
    sixel_mutex_lock(&pool->mutex);
    if (pool->shutting_down) {
        sixel_mutex_unlock(&pool->mutex);
        return;
    }
    while (pool->count == pool->qsize && !pool->shutting_down) {
        sixel_cond_wait(&pool->cond_not_full, &pool->mutex);
    }
    if (pool->shutting_down) {
        sixel_mutex_unlock(&pool->mutex);
        return;
    }
    pool->jobs[pool->tail] = job;
    pool->tail = (pool->tail + 1) % pool->qsize;
    pool->count += 1;
    sixel_cond_signal(&pool->cond_not_empty);
    sixel_mutex_unlock(&pool->mutex);
}

SIXELAPI void
threadpool_finish(threadpool_t *pool)
{
    int i;

    if (pool == NULL) {
        return;
    }
    sixel_mutex_lock(&pool->mutex);
    if (pool->joined) {
        sixel_mutex_unlock(&pool->mutex);
        return;
    }
    pool->shutting_down = 1;
    sixel_cond_broadcast(&pool->cond_not_empty);
    sixel_cond_broadcast(&pool->cond_not_full);
    while (pool->count > 0 || pool->running > 0) {
        sixel_cond_wait(&pool->cond_drained, &pool->mutex);
    }
    sixel_mutex_unlock(&pool->mutex);

    for (i = 0; i < pool->threads_started; ++i) {
        if (pool->workers[i].started) {
            sixel_thread_join(&pool->workers[i].thread);
            pool->workers[i].started = 0;
        }
    }

    sixel_mutex_lock(&pool->mutex);
    pool->joined = 1;
    sixel_mutex_unlock(&pool->mutex);
}

SIXELAPI int
threadpool_get_error(threadpool_t *pool)
{
    int error;

    if (pool == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_mutex_lock(&pool->mutex);
    error = pool->error;
    sixel_mutex_unlock(&pool->mutex);
    return error;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
