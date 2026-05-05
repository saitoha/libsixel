/*
 * SPDX-License-Identifier: MIT
 *
 * Unit test for the threadpool service and short-lived pool component.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

typedef struct sixel_thread_pool_test_context {
    int seen[8];
    int pool_id;
} sixel_thread_pool_test_context_t;

typedef struct sixel_thread_pool_test_workspace {
    int touched;
} sixel_thread_pool_test_workspace_t;

static int g_sixel_thread_pool_cleanup_count;

static int
sixel_thread_pool_test_worker(sixel_thread_pool_job_t job,
                              void *userdata,
                              void *workspace)
{
    sixel_thread_pool_test_context_t *context;
    sixel_thread_pool_test_workspace_t *local;

    context = (sixel_thread_pool_test_context_t *)userdata;
    local = (sixel_thread_pool_test_workspace_t *)workspace;
    if (context == NULL || local == NULL ||
        job.band_index < 0 || job.band_index >= 8) {
        return SIXEL_BAD_ARGUMENT;
    }

    local->touched = 1;
    context->seen[job.band_index] = context->pool_id;
    return SIXEL_OK;
}

static void
sixel_thread_pool_test_workspace_cleanup(void *workspace)
{
    sixel_thread_pool_test_workspace_t *local;

    local = (sixel_thread_pool_test_workspace_t *)workspace;
    if (local != NULL) {
        g_sixel_thread_pool_cleanup_count += 1;
    }
}

static int
sixel_thread_pool_test_sum(sixel_thread_pool_test_context_t const *context,
                           int expected)
{
    int index;

    for (index = 0; index < 8; ++index) {
        if (context->seen[index] != expected) {
            return 0;
        }
    }
    return 1;
}

static int
sixel_thread_pool_test_service_component(void)
{
    SIXELSTATUS status;
    sixel_threadpool_service_t *service;
    sixel_thread_pool_t *pool1;
    sixel_thread_pool_t *pool2;
    sixel_thread_pool_create_request_t request;
    sixel_thread_pool_test_context_t context1;
    sixel_thread_pool_test_context_t context2;
    sixel_thread_pool_job_t job;
    void *service_object;
    int index;
    int resolved;

    status = SIXEL_FALSE;
    service = NULL;
    pool1 = NULL;
    pool2 = NULL;
    service_object = NULL;
    memset(&request, 0, sizeof(request));
    memset(&context1, 0, sizeof(context1));
    memset(&context2, 0, sizeof(context2));
    memset(&job, 0, sizeof(job));
    g_sixel_thread_pool_cleanup_count = 0;

    status = sixel_components_getservice("services/threadpool",
                                         &service_object);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    service = (sixel_threadpool_service_t *)service_object;
    service_object = NULL;
    if (service == NULL || service->vtbl == NULL ||
        service->vtbl->resolve_threads == NULL ||
        service->vtbl->create_pool == NULL ||
        service->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    resolved = service->vtbl->resolve_threads(service, 0);
    if (resolved < 1) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    request.threads = 2;
    request.queue_size = 4;
    request.workspace_size = sizeof(sixel_thread_pool_test_workspace_t);
    request.worker = sixel_thread_pool_test_worker;
    request.userdata = &context1;
    request.workspace_cleanup = sixel_thread_pool_test_workspace_cleanup;

#if SIXEL_ENABLE_THREADS
    status = service->vtbl->create_pool(service, &request, &pool1);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    request.userdata = &context2;
    status = service->vtbl->create_pool(service, &request, &pool2);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    context1.pool_id = 1;
    context2.pool_id = 2;
    for (index = 0; index < 8; ++index) {
        job.band_index = index;
        status = pool1->vtbl->push(pool1, job);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = pool2->vtbl->push(pool2, job);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    pool1->vtbl->finish(pool1);
    pool2->vtbl->finish(pool2);
    if (pool1->vtbl->get_error(pool1) != SIXEL_OK ||
        pool2->vtbl->get_error(pool2) != SIXEL_OK) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    if (!sixel_thread_pool_test_sum(&context1, 1) ||
        !sixel_thread_pool_test_sum(&context2, 2)) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    pool1->vtbl->unref(pool1);
    pool1 = NULL;
    pool2->vtbl->unref(pool2);
    pool2 = NULL;
    if (g_sixel_thread_pool_cleanup_count != 4) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
#else
    status = service->vtbl->create_pool(service, &request, &pool1);
    if (status != SIXEL_NOT_IMPLEMENTED || pool1 != NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
#endif

    status = SIXEL_OK;

end:
    if (pool2 != NULL && pool2->vtbl != NULL &&
        pool2->vtbl->unref != NULL) {
        pool2->vtbl->unref(pool2);
    }
    if (pool1 != NULL && pool1->vtbl != NULL &&
        pool1->vtbl->unref != NULL) {
        pool1->vtbl->unref(pool1);
    }
    if (service != NULL && service->vtbl != NULL &&
        service->vtbl->unref != NULL) {
        service->vtbl->unref(service);
    }
    return SIXEL_SUCCEEDED(status);
}

int
test_threadpool_0001_threadpool_service(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!sixel_thread_pool_test_service_component()) {
        fprintf(stderr, "threadpool service component contract failed\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
