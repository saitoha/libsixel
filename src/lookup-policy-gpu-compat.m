/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_METAL)
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "lookup-policy-gpu.h"
#include "gpu-palette.h"
#include "palette.h"

/* This compatibility composition root translates the old request into
 * injected capabilities. The fused runtime has no policy registry dependency.
 * Calls through the old synchronous API retain their serialized cache reuse. */
typedef struct gpu_compat_provider {
    int kind;
    sixel_gpu_lookup_preparer_interface_t *provider;
    struct gpu_compat_provider *next;
} gpu_compat_provider_t;

static pthread_mutex_t gpu_compat_gate = PTHREAD_MUTEX_INITIALIZER;
static gpu_compat_provider_t *gpu_compat_providers;

SIXELSTATUS sixel_gpu_palette_metal_apply(
    sixel_gpu_palette_request_t const *request);

SIXELSTATUS
sixel_gpu_lookup_compat_apply(sixel_gpu_palette_request_t const *request)
{
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    sixel_palette_entries_request_t entries;
    sixel_lookup_execution_request_t execution;
    sixel_lookup_policy_configuration_t configuration;
    sixel_gpu_lookup_plan_interface_t *plan;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_program_t *program;
    sixel_gpu_palette_request_t run;
    gpu_compat_provider_t *cached;
    void *object;
    SIXELSTATUS status;

    if (request->program != NULL ||
            !sixel_gpu_lookup_registered(request->lut_policy)) {
        return sixel_gpu_palette_metal_apply(request);
    }
    allocator = NULL;
    palette = NULL;
    plan = NULL;
    device = NULL;
    program = NULL;
    object = NULL;
    memset(&configuration, 0, sizeof(configuration));
    memset(&execution, 0, sizeof(execution));
    (void)pthread_mutex_lock(&gpu_compat_gate);
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) goto end;
    cached = gpu_compat_providers;
    while (cached != NULL && cached->kind != request->lut_policy) {
        cached = cached->next;
    }
    if (cached == NULL) {
        cached = calloc(1, sizeof(*cached));
        if (cached == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        configuration.kind = request->lut_policy;
        configuration.pixelformat = request->pixelformat;
        status = sixel_gpu_lookup_select(allocator, &configuration,
            &cached->provider);
        if (status != SIXEL_OK) {
            free(cached);
            goto end;
        }
        cached->kind = request->lut_policy;
        cached->next = gpu_compat_providers;
        gpu_compat_providers = cached;
    }
    status = sixel_palette_factory_new(allocator, &object);
    if (SIXEL_FAILED(status)) goto end;
    palette = object;
    entries.entries = request->palette;
    entries.colors = (unsigned int)request->ncolors;
    entries.depth = 3;
    status = palette->vtbl->init_entries(palette, &entries);
    if (SIXEL_FAILED(status)) goto end;
    execution.working_palette = palette;
    execution.ncolors = entries.colors;
    execution.pixelformat = request->pixelformat;
    execution.colorspace = SIXEL_COLORSPACE_GAMMA;
    status = sixel_gpu_device_acquire(&device);
    if (SIXEL_FAILED(status)) goto end;
    status = cached->provider->vtbl->prepare(cached->provider, &execution,
        device, &plan);
    if (SIXEL_FAILED(status)) goto end;
    status = sixel_gpu_program_prepare(device, plan, &program);
    if (SIXEL_FAILED(status)) goto end;
    run = *request;
    run.program = program;
    status = sixel_gpu_palette_metal_apply(&run);
end:
    sixel_gpu_program_destroy(program);
    if (plan != NULL) plan->vtbl->unref(plan);
    if (device != NULL) device->vtbl->unref(device);
    if (palette != NULL) palette->vtbl->unref(palette);
    sixel_allocator_unref(allocator);
    (void)pthread_mutex_unlock(&gpu_compat_gate);
    return status;
}
#endif
