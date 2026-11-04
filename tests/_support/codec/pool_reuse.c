/* SPDX-License-Identifier: MIT */
/* One selected observation per wrapper; a single worker makes workspace and
 * callback-history assertions deterministic without sleeps or polling. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include <6cells.h>
#include "src/threading.h"

#if SIXEL_ENABLE_THREADS
typedef struct pool_probe {
    int calls;
    int fail;
    int value;
    int mode;
    int budget;
    int hooks;
    int expired;
    int late;
    sixel_thread_identity_t identity[2];
} pool_probe_t;

static int pool_probe_cleanups;

static void
pool_probe_cleanup(void *workspace)
{
    (void)workspace;
    ++pool_probe_cleanups;
}

static void
pool_probe_hook(void *context, int worker, int begin)
{
    pool_probe_t *p;

    (void)begin;
    if (!worker) return;
    p = context;
    ++p->hooks;
    if (p->expired) ++p->late;
}

static int
pool_probe_job(sixel_thread_pool_job_t job, void *context, void *workspace)
{
    pool_probe_t *p;
    int *value;

    p = context;
    value = workspace;
    p->value = ++*value;
    p->mode = sixel_threading_policy_mode();
    p->budget = sixel_threads_resolve();
    if (job.band_index < 2) {
        p->identity[job.band_index] = sixel_thread_identity_current();
    }
    ++p->calls;
    return p->fail ? SIXEL_BAD_ALLOCATION : SIXEL_OK;
}
#endif

int
pool_reuse_observe(int observation)
{
#if SIXEL_ENABLE_THREADS
    sixel_threadpool_service_t *service;
    sixel_thread_pool_t *pool;
    sixel_thread_pool_create_request_t request;
    sixel_thread_pool_job_t job;
    sixel_threading_snapshot_t policy;
    sixel_thread_budget_scope_t scope;
    pool_probe_t p;
    pool_probe_t hooks[2];
    void *object;
    SIXELSTATUS status;
    int scoped;
    int ok;

    pool = NULL;
    service = NULL;
    scoped = 0;
    ok = 0;
    memset(&p, 0, sizeof(p));
    memset(hooks, 0, sizeof(hooks));
    memset(&request, 0, sizeof(request));
    pool_probe_cleanups = 0;
    object = NULL;
    if (sixel_components_getservice("services/threadpool", &object) !=
            SIXEL_OK) goto end;
    service = object;
    (void)sixel_threading_policy_capture(&policy, 0);
    policy.options.mode = SIXEL_THREADING_POLICY_LAZY;
    policy.budget = 4;
    policy.work = pool_probe_hook;
    policy.work_context = &hooks[0];
    sixel_threading_policy_enter(&scope, &policy);
    scoped = 1;
    request.threads = request.queue_size = 1;
    request.workspace_size = sizeof(int);
    request.worker = pool_probe_job;
    request.workspace_cleanup = pool_probe_cleanup;
    request.userdata = &p;
    if (service->vtbl->create_pool(service, &request, &pool) != SIXEL_OK)
        goto end;
    if (observation == 6) {
        ok = pool->vtbl->resume(pool) == SIXEL_LOGIC_ERROR;
        goto end;
    }
    p.fail = observation == 8 || observation == 9;
    job.band_index = 0;
    if (pool->vtbl->push(pool, job) != SIXEL_OK) goto end;
    if (observation == 13) {
        pool->vtbl->finish(pool);
        ok = p.calls == 1;
        goto end;
    }
    status = pool->vtbl->drain(pool);
    if (status != (p.fail ? SIXEL_BAD_ALLOCATION : SIXEL_OK)) goto end;
    sixel_thread_budget_leave(&scope);
    scoped = 0;
    hooks[0].expired = 1;
    switch (observation) {
    case 0: ok = p.calls == 1; goto end;
    case 5: ok = pool->vtbl->push(pool, job) == SIXEL_LOGIC_ERROR; goto end;
    case 7:
        pool->vtbl->finish(pool);
        ok = pool->vtbl->resume(pool) == SIXEL_LOGIC_ERROR;
        goto end;
    case 8: ok = pool->vtbl->resume(pool) == SIXEL_LOGIC_ERROR; goto end;
    case 9:
        ok = pool->vtbl->drain(pool) == SIXEL_BAD_ALLOCATION;
        goto end;
    case 11: ok = pool->vtbl->grow(pool, 1) != SIXEL_OK; goto end;
    case 12: ok = pool->vtbl->drain(pool) == SIXEL_OK; goto end;
    default: break;
    }
    policy.options.mode = SIXEL_THREADING_POLICY_GREEDY;
    policy.work_context = &hooks[1];
    sixel_threading_policy_enter(&scope, &policy);
    scoped = 1;
    if (pool->vtbl->resume(pool) != SIXEL_OK) goto end;
    job.band_index = 1;
    if (pool->vtbl->push(pool, job) != SIXEL_OK ||
            pool->vtbl->drain(pool) != SIXEL_OK) goto end;
    switch (observation) {
    case 1: ok = p.value == 2; break;
    case 2:
        ok = sixel_thread_identity_equal(p.identity[0], p.identity[1]);
        break;
    case 3: ok = p.mode == SIXEL_THREADING_POLICY_GREEDY; break;
    case 4:
        ok = hooks[0].hooks == 2 && hooks[0].late == 0 &&
            hooks[1].hooks == 2;
        break;
    case 10:
        pool->vtbl->unref(pool);
        pool = NULL;
        ok = pool_probe_cleanups == 1;
        break;
    case 14: ok = p.budget == 1; break;
    default: break;
    }
end:
    if (pool != NULL) pool->vtbl->unref(pool);
    if (scoped) sixel_thread_budget_leave(&scope);
    if (service != NULL) service->vtbl->unref(service);
    return ok ? 0 : 1;
#else
    (void)observation;
    return 77;
#endif
}
