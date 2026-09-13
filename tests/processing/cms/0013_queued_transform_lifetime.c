/* SPDX-License-Identifier: MIT */
/* Queued work retains its transform after temporary endpoints are freed. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/cms.h"
#include "src/colorspace.h"
#include "tests/data/color-numeric/vectors.h"

#if SIXEL_ENABLE_THREADS
static int
color_job(sixel_thread_pool_job_t job, void *userdata, void *workspace)
{
    sixel_color_xform_t const *transform;
    double rgb[3];
    int channel;

    (void)workspace;
    transform = userdata;
    if (!sixel_cxform_apply(transform, color_vectors[job.band_index],
                            rgb)) {
        return SIXEL_RUNTIME_ERROR;
    }
    for (channel = 0; channel < 3; ++channel) {
        if (fabs(sixel_color_decode_srgb(rgb[channel]) -
            color_vectors[job.band_index][9 + channel]) > 1e-10) {
            return SIXEL_RUNTIME_ERROR;
        }
    }
    return SIXEL_OK;
}
#endif

int test_cdef_0013(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    sixel_color_def_t *source;
    sixel_color_def_t *destination;
    sixel_color_xform_t *transform;
    sixel_threadpool_service_t *service;
    sixel_thread_pool_t *pool;
    sixel_thread_pool_create_request_t request;
    sixel_thread_pool_job_t job;
    void *object;
    int ok;

    (void)argc;
    (void)argv;
    source = sixel_cdef_name("display-p3");
    destination = sixel_cdef_name("srgb");
    transform = sixel_cxform_new(source, destination, 1);
    sixel_cdef_free(source);
    sixel_cdef_free(destination);
    object = NULL;
    service = NULL;
    pool = NULL;
    ok = transform != NULL && SIXEL_SUCCEEDED(sixel_components_getservice(
        "services/threadpool", &object));
    if (ok) {
        service = object;
        memset(&request, 0, sizeof(request));
        request.threads = 2;
        request.queue_size = 4;
        request.worker = color_job;
        request.userdata = transform;
        ok = SIXEL_SUCCEEDED(service->vtbl->create_pool(
            service, &request, &pool));
    }
    for (job.band_index = 0; ok && job.band_index < 7; ++job.band_index) {
        ok = SIXEL_SUCCEEDED(pool->vtbl->push(pool, job));
    }
    if (pool != NULL) {
        pool->vtbl->finish(pool);
        ok = ok && pool->vtbl->get_error(pool) == SIXEL_OK;
        pool->vtbl->unref(pool);
    }
    if (service != NULL) {
        service->vtbl->unref(service);
    }
    sixel_cxform_free(transform);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
