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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "threadpool.h"
#include "threading.h"

#if SIXEL_ENABLE_THREADS

#include "sixel_atomic.h"

typedef struct sixel_thread_pool_worker sixel_thread_pool_worker_t;
typedef struct sixel_thread_pool_storage sixel_thread_pool_storage_t;

static sixel_thread_pool_vtbl_t const sixel_thread_pool_vtbl;

struct sixel_thread_pool_worker {
    sixel_thread_pool_storage_t *pool;
    sixel_thread_t thread;
    void *workspace;
    int started;
    int index;
    int pinned;
};

struct sixel_thread_pool_storage {
    sixel_thread_pool_vtbl_t const *vtbl;
    sixel_atomic_u32_t ref;
    int nthreads;
    int qsize;
    size_t workspace_size;
    sixel_thread_pool_workspace_cleanup_function_t workspace_cleanup;
    sixel_thread_pool_worker_function_t worker;
    void *userdata;
    sixel_thread_pool_job_t *jobs;
    int head;
    int tail;
    int count;
    int running;
    int shutting_down;
    int joined;
    int error;
    int threads_started;
    int worker_capacity;
    int pin_threads;
    int hw_threads;
    sixel_mutex_t mutex;
    sixel_cond_t cond_not_empty;
    sixel_cond_t cond_not_full;
    sixel_cond_t cond_drained;
    int mutex_ready;
    int cond_not_empty_ready;
    int cond_not_full_ready;
    int cond_drained_ready;
    sixel_thread_pool_worker_t **workers; /* owned stable worker slots */
};

static void sixel_thread_pool_storage_free(sixel_thread_pool_storage_t *pool);
static int sixel_thread_pool_worker_main(void *arg);
static int sixel_thread_pool_spawn_worker(sixel_thread_pool_storage_t *pool,
                                          sixel_thread_pool_worker_t *worker);
static void sixel_thread_pool_storage_finish(sixel_thread_pool_storage_t *pool);

/*
 * Release every dynamically allocated component of the pool. Callers must
 * ensure that worker threads have already terminated before invoking this
 * helper; otherwise joining would operate on freed memory.
 */
static void
sixel_thread_pool_storage_free(sixel_thread_pool_storage_t *pool)
{
    int i;

    if (pool == NULL) {
        return;
    }
    if (pool->workers != NULL) {
        for (i = 0; i < pool->worker_capacity; ++i) {
            sixel_thread_pool_worker_t *worker;
            worker = pool->workers[i];
            if (worker == NULL) {
                continue;
            }
            if (worker->workspace != NULL) {
                if (pool->workspace_cleanup != NULL) {
                    pool->workspace_cleanup(worker->workspace);
                }
                free(worker->workspace);
            }
            free(worker);
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
sixel_thread_pool_worker_main(void *arg)
{
    sixel_thread_pool_worker_t *worker;
    sixel_thread_pool_storage_t *pool;
    sixel_thread_pool_job_t job;
    int rc;

    worker = (sixel_thread_pool_worker_t *)arg;
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

        if (pool->pin_threads && !worker->pinned && pool->hw_threads > 0) {
            int cpu_index;

            cpu_index = worker->index % pool->hw_threads;
            (void)sixel_thread_pin_self(cpu_index);
            worker->pinned = 1;
        }

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

static sixel_thread_pool_storage_t *
sixel_thread_pool_storage_create(int nthreads,
                                 int qsize,
                                 size_t workspace_size,
                                 sixel_thread_pool_worker_function_t worker,
                                 void *userdata,
                                 sixel_thread_pool_workspace_cleanup_function_t
                                    workspace_cleanup)
{
    sixel_thread_pool_storage_t *pool;
    int i;
    int rc;

    if (nthreads <= 0 || qsize <= 0 || worker == NULL) {
        return NULL;
    }
    pool = (sixel_thread_pool_storage_t *)
        calloc(1, sizeof(sixel_thread_pool_storage_t));
    if (pool == NULL) {
        return NULL;
    }
    pool->vtbl = &sixel_thread_pool_vtbl;
    pool->ref = 1U;
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
    pool->pin_threads = 0;
    pool->hw_threads = 0;
    pool->workers = NULL;
    pool->workspace_cleanup = workspace_cleanup;

    rc = sixel_mutex_init(&pool->mutex);
    if (rc != SIXEL_OK) {
        errno = EINVAL;
        sixel_thread_pool_storage_free(pool);
        return NULL;
    }
    pool->mutex_ready = 1;

    rc = sixel_cond_init(&pool->cond_not_empty);
    if (rc != SIXEL_OK) {
        errno = EINVAL;
        sixel_thread_pool_storage_free(pool);
        return NULL;
    }
    pool->cond_not_empty_ready = 1;

    rc = sixel_cond_init(&pool->cond_not_full);
    if (rc != SIXEL_OK) {
        errno = EINVAL;
        sixel_thread_pool_storage_free(pool);
        return NULL;
    }
    pool->cond_not_full_ready = 1;

    rc = sixel_cond_init(&pool->cond_drained);
    if (rc != SIXEL_OK) {
        errno = EINVAL;
        sixel_thread_pool_storage_free(pool);
        return NULL;
    }
    pool->cond_drained_ready = 1;

    pool->jobs = (sixel_thread_pool_job_t *)
        malloc(sizeof(sixel_thread_pool_job_t) * (size_t)qsize);
    if (pool->jobs == NULL) {
        sixel_thread_pool_storage_free(pool);
        return NULL;
    }

    pool->worker_capacity = nthreads;
    pool->workers = (sixel_thread_pool_worker_t **)calloc((size_t)nthreads,
            sizeof(sixel_thread_pool_worker_t *));
    if (pool->workers == NULL) {
        sixel_thread_pool_storage_free(pool);
        return NULL;
    }

    for (i = 0; i < nthreads; ++i) {
        pool->workers[i] = (sixel_thread_pool_worker_t *)
            calloc(1, sizeof(sixel_thread_pool_worker_t));
        if (pool->workers[i] == NULL) {
            pool->shutting_down = 1;
            sixel_cond_broadcast(&pool->cond_not_empty);
            break;
        }
        pool->workers[i]->pool = pool;
        pool->workers[i]->workspace = NULL;
        pool->workers[i]->started = 0;
        pool->workers[i]->index = i;
        pool->workers[i]->pinned = 0;
        if (workspace_size > 0) {
            /*
             * Zero-initialize the per-thread workspace so that structures like
             * `sixel_parallel_worker_state_t` start with predictable values.
             * The worker initialization logic assumes fields such as
             * `initialized` are cleared before the first job.
             */
            pool->workers[i]->workspace = calloc(1, workspace_size);
            if (pool->workers[i]->workspace == NULL) {
                pool->shutting_down = 1;
                sixel_cond_broadcast(&pool->cond_not_empty);
                break;
            }
        }
        rc = sixel_thread_pool_spawn_worker(pool, pool->workers[i]);
        if (rc != SIXEL_OK) {
            break;
        }
    }

    if (pool->threads_started != nthreads) {
        int started;

        started = pool->threads_started;
        for (i = 0; i < started; ++i) {
            sixel_cond_broadcast(&pool->cond_not_empty);
            sixel_thread_join(&pool->workers[i]->thread);
        }
        sixel_thread_pool_storage_free(pool);
        return NULL;
    }

    return pool;
}

static void
sixel_thread_pool_storage_set_affinity(sixel_thread_pool_storage_t *pool,
                                       int pin_threads)
{
    if (pool == NULL) {
        return;
    }

    sixel_mutex_lock(&pool->mutex);
    pool->pin_threads = (pin_threads != 0) ? 1 : 0;
    if (pool->pin_threads != 0) {
        pool->hw_threads = sixel_get_hw_threads();
        if (pool->hw_threads < 1) {
            pool->pin_threads = 0;
        }
    } else {
        pool->hw_threads = 0;
    }
    sixel_mutex_unlock(&pool->mutex);
}

static int
sixel_thread_pool_spawn_worker(sixel_thread_pool_storage_t *pool,
                               sixel_thread_pool_worker_t *worker)
{
    int rc;

    if (pool == NULL || worker == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    rc = sixel_thread_create(&worker->thread,
                             sixel_thread_pool_worker_main,
                             worker);
    if (rc != SIXEL_OK) {
        sixel_mutex_lock(&pool->mutex);
        pool->shutting_down = 1;
        sixel_cond_broadcast(&pool->cond_not_empty);
        sixel_mutex_unlock(&pool->mutex);
        return rc;
    }
    worker->started = 1;
    pool->threads_started += 1;
    return SIXEL_OK;
}

static void
sixel_thread_pool_storage_destroy(sixel_thread_pool_storage_t *pool)
{
    if (pool == NULL) {
        return;
    }
    sixel_thread_pool_storage_finish(pool);
    sixel_thread_pool_storage_free(pool);
}

static void
sixel_thread_pool_storage_push(sixel_thread_pool_storage_t *pool,
                               sixel_thread_pool_job_t job)
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

static void
sixel_thread_pool_storage_finish(sixel_thread_pool_storage_t *pool)
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
        if (pool->workers[i] != NULL && pool->workers[i]->started) {
            sixel_thread_join(&pool->workers[i]->thread);
            pool->workers[i]->started = 0;
        }
    }

    sixel_mutex_lock(&pool->mutex);
    pool->joined = 1;
    sixel_mutex_unlock(&pool->mutex);
}

static int
sixel_thread_pool_storage_grow(sixel_thread_pool_storage_t *pool,
                               int additional_threads)
{
    sixel_thread_pool_worker_t **expanded;
    int new_target;
    int started_new;
    int i;
    int rc;

    if (pool == NULL || additional_threads <= 0) {
        return SIXEL_OK;
    }

    sixel_mutex_lock(&pool->mutex);
    if (pool->shutting_down) {
        sixel_mutex_unlock(&pool->mutex);
        return SIXEL_RUNTIME_ERROR;
    }
    new_target = pool->nthreads + additional_threads;
    /*
     * Worker structs stay heap-allocated per slot so pointer-table growth
     * never invalidates addresses already held by running threads.
     */
    if (new_target > pool->worker_capacity) {
        expanded = (sixel_thread_pool_worker_t **)realloc(
            pool->workers,
            (size_t)new_target * sizeof(sixel_thread_pool_worker_t *));
        if (expanded == NULL) {
            sixel_mutex_unlock(&pool->mutex);
            return SIXEL_BAD_ALLOCATION;
        }
        memset(expanded + pool->worker_capacity,
               0,
               (size_t)(new_target - pool->worker_capacity)
                   * sizeof(sixel_thread_pool_worker_t *));
        pool->workers = expanded;
        pool->worker_capacity = new_target;
    }
    sixel_mutex_unlock(&pool->mutex);

    started_new = 0;
    rc = SIXEL_OK;
    for (i = pool->nthreads; i < new_target; ++i) {
        pool->workers[i] = (sixel_thread_pool_worker_t *)
            calloc(1, sizeof(sixel_thread_pool_worker_t));
        if (pool->workers[i] == NULL) {
            rc = SIXEL_BAD_ALLOCATION;
            break;
        }
        pool->workers[i]->pool = pool;
        pool->workers[i]->workspace = NULL;
        pool->workers[i]->started = 0;
        pool->workers[i]->index = i;
        pool->workers[i]->pinned = 0;
        if (pool->workspace_size > 0) {
            pool->workers[i]->workspace =
                calloc(1, pool->workspace_size);
            if (pool->workers[i]->workspace == NULL) {
                rc = SIXEL_BAD_ALLOCATION;
                break;
            }
        }

        rc = sixel_thread_pool_spawn_worker(pool, pool->workers[i]);
        if (rc != SIXEL_OK) {
            break;
        }
        started_new += 1;
    }

    if (rc != SIXEL_OK) {
        int j;

        for (j = i; j < new_target; ++j) {
            if (pool->workers[j] != NULL) {
                if (pool->workers[j]->workspace != NULL) {
                    free(pool->workers[j]->workspace);
                }
                free(pool->workers[j]);
                pool->workers[j] = NULL;
            }
        }
    }

    sixel_mutex_lock(&pool->mutex);
    pool->nthreads = pool->nthreads + started_new;
    sixel_mutex_unlock(&pool->mutex);

    return rc;
}

static int
sixel_thread_pool_storage_get_error(sixel_thread_pool_storage_t *pool)
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

static sixel_thread_pool_storage_t *
sixel_thread_pool_from_interface(sixel_thread_pool_t *self)
{
    return (sixel_thread_pool_storage_t *)self;
}

static void
sixel_thread_pool_ref(sixel_thread_pool_t *self)
{
    sixel_thread_pool_storage_t *pool;

    pool = sixel_thread_pool_from_interface(self);
    if (pool == NULL) {
        return;
    }
    (void)sixel_atomic_fetch_add_u32(&pool->ref, 1U);
}

static void
sixel_thread_pool_unref(sixel_thread_pool_t *self)
{
    sixel_thread_pool_storage_t *pool;
    unsigned int previous;

    pool = sixel_thread_pool_from_interface(self);
    if (pool == NULL) {
        return;
    }
    previous = sixel_atomic_fetch_sub_u32(&pool->ref, 1U);
    if (previous == 1U) {
        sixel_thread_pool_storage_destroy(pool);
    }
}

static void
sixel_thread_pool_set_affinity(sixel_thread_pool_t *self, int pin_threads)
{
    sixel_thread_pool_storage_set_affinity(
        sixel_thread_pool_from_interface(self),
        pin_threads);
}

static SIXELSTATUS
sixel_thread_pool_push(sixel_thread_pool_t *self,
                       sixel_thread_pool_job_t job)
{
    sixel_thread_pool_storage_t *pool;

    pool = sixel_thread_pool_from_interface(self);
    if (pool == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_thread_pool_storage_push(pool, job);
    return SIXEL_OK;
}

static void
sixel_thread_pool_finish(sixel_thread_pool_t *self)
{
    sixel_thread_pool_storage_finish(sixel_thread_pool_from_interface(self));
}

static int
sixel_thread_pool_get_error(sixel_thread_pool_t *self)
{
    return sixel_thread_pool_storage_get_error(
        sixel_thread_pool_from_interface(self));
}

static SIXELSTATUS
sixel_thread_pool_grow(sixel_thread_pool_t *self, int additional_threads)
{
    return sixel_thread_pool_storage_grow(
        sixel_thread_pool_from_interface(self),
        additional_threads);
}

static sixel_thread_pool_vtbl_t const sixel_thread_pool_vtbl = {
    sixel_thread_pool_ref,
    sixel_thread_pool_unref,
    sixel_thread_pool_set_affinity,
    sixel_thread_pool_push,
    sixel_thread_pool_finish,
    sixel_thread_pool_get_error,
    sixel_thread_pool_grow
};

#endif  /* SIXEL_ENABLE_THREADS */

typedef struct sixel_threadpool_service_storage {
    sixel_threadpool_service_vtbl_t const *vtbl;
} sixel_threadpool_service_storage_t;

static void
sixel_threadpool_service_ref(sixel_threadpool_service_t *service)
{
    (void)service;
}

static void
sixel_threadpool_service_unref(sixel_threadpool_service_t *service)
{
    (void)service;
}

static int
sixel_threadpool_service_resolve_threads(
    sixel_threadpool_service_t *service,
    int requested)
{
    (void)service;
    return sixel_threads_normalize(requested);
}

static SIXELSTATUS
sixel_threadpool_service_create_pool(
    sixel_threadpool_service_t *service,
    sixel_thread_pool_create_request_t const *request,
    sixel_thread_pool_t **pool)
{
    (void)service;
    if (pool != NULL) {
        *pool = NULL;
    }
    if (request == NULL || pool == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
#if SIXEL_ENABLE_THREADS
    if (request->threads <= 0 || request->queue_size <= 0 ||
        request->worker == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *pool = (sixel_thread_pool_t *)sixel_thread_pool_storage_create(
        request->threads,
        request->queue_size,
        request->workspace_size,
        request->worker,
        request->userdata,
        request->workspace_cleanup);
    if (*pool == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    return SIXEL_OK;
#else
    return SIXEL_NOT_IMPLEMENTED;
#endif
}

static sixel_threadpool_service_vtbl_t const
g_sixel_threadpool_service_vtbl = {
    sixel_threadpool_service_ref,
    sixel_threadpool_service_unref,
    sixel_threadpool_service_resolve_threads,
    sixel_threadpool_service_create_pool
};

static sixel_threadpool_service_storage_t g_sixel_threadpool_service = {
    &g_sixel_threadpool_service_vtbl
};

SIXELSTATUS
sixel_threadpool_service_get_default(void **service)
{
    sixel_threadpool_service_t *threadpool_service;

    if (service == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    threadpool_service =
        (sixel_threadpool_service_t *)&g_sixel_threadpool_service;
    threadpool_service->vtbl->ref(threadpool_service);
    *service = threadpool_service;
    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
