/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "lookup-policy-gpu.h"
#include "gpu-palette.h"
#include "dither-policy-gpu.h"
#include "palette.h"

/* This compatibility composition root translates the old request into
 * injected capabilities. The fused runtime has no policy registry dependency.
 * Calls through the old synchronous API retain their serialized cache reuse. */
typedef struct gpu_compat_provider {
    int kind;
    sixel_lookup_policy_configuration_t configuration;
    sixel_gpu_lookup_preparer_interface_t *provider;
    struct gpu_compat_provider *next;
} gpu_compat_provider_t;

static pthread_mutex_t gpu_compat_gate = PTHREAD_MUTEX_INITIALIZER;
static gpu_compat_provider_t *gpu_compat_providers;

/* The backend runtime that executes a prepared program. */
#if defined(HAVE_METAL)
SIXELSTATUS sixel_gpu_palette_metal_apply(
    sixel_gpu_palette_request_t const *request);
#define gpu_compat_run sixel_gpu_palette_metal_apply
#else
#include "gpu-palette-vulkan.h"
#define gpu_compat_run sixel_gpu_palette_vulkan_apply
#endif

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
    sixel_gpu_lookup_preparer_interface_t *provider;
    void *object;
    SIXELSTATUS status;

    run = *request;
    if (run.dither_program == NULL)
        run.dither_program = sixel_gpu_dither_byte_program(
            run.method_for_diffuse);
    if (run.dither_program == NULL) return SIXEL_FEATURE_ERROR;
    if (request->program != NULL) {
        return gpu_compat_run(&run);
    }
    /* The synchronous AUTO contract uses exhaustive lookup. Resolve it
     * here so the native runtime always receives an owned program; no
     * explicitly selected policy may fall through to a different search. */
    if (run.lut_policy == SIXEL_LUT_POLICY_AUTO)
        run.lut_policy = SIXEL_LUT_POLICY_NONE;
    if (!sixel_gpu_lookup_registered(run.lut_policy))
        return SIXEL_FEATURE_ERROR;
    /* Legacy callers consume a whole synchronous result. Incremental row
     * publication belongs to a caller-owned prepared program and its lease. */
    run.rows_per_chunk = 0;
    run.rows_ready = NULL;
    allocator = NULL;
    palette = NULL;
    plan = NULL;
    device = NULL;
    program = NULL;
    object = NULL;
    memset(&configuration, 0, sizeof(configuration));
    configuration.kind = run.lut_policy;
    configuration.pixelformat = request->pixelformat;
    configuration.fhedt_resolution = request->fhedt_resolution;
    configuration.fhedt_refine = request->fhedt_refine;
    configuration.fhedt_use_dist2 = request->fhedt_use_dist2;
    configuration.fhedt_use_cache = request->fhedt_use_cache;
    memcpy(configuration.fhedt_weights, request->fhedt_weights,
        sizeof(configuration.fhedt_weights));
    memset(&execution, 0, sizeof(execution));
    /* The byte executor accepts both point and collective programs. */
    execution.allow_batch = 1;
    (void)pthread_mutex_lock(&gpu_compat_gate);
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) goto end;
    cached = gpu_compat_providers;
    while (cached != NULL && cached->kind != configuration.kind) {
        cached = cached->next;
    }
    if (cached == NULL) {
        cached = calloc(1, sizeof(*cached));
        if (cached == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        status = sixel_gpu_lookup_select(allocator, &configuration,
            &cached->provider);
        if (status != SIXEL_OK) {
            free(cached);
            goto end;
        }
        cached->kind = configuration.kind;
        cached->configuration = configuration;
        cached->next = gpu_compat_providers;
        gpu_compat_providers = cached;
    } else if (memcmp(&cached->configuration, &configuration,
            sizeof(configuration)) != 0) {
        /* Providers freeze resolved settings. Replace only after successful
         * construction, retaining at most one idle provider per policy. */
        status = sixel_gpu_lookup_select(allocator, &configuration, &provider);
        if (status != SIXEL_OK) goto end;
        cached->provider->vtbl->unref(cached->provider);
        cached->provider = provider;
        cached->configuration = configuration;
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
    status = sixel_gpu_program_prepare(device, plan,
        run.dither_program, &program);
    if (SIXEL_FAILED(status)) goto end;
    run.program = program;
    status = gpu_compat_run(&run);
end:
    sixel_gpu_program_destroy(program);
    if (plan != NULL) plan->vtbl->unref(plan);
    if (device != NULL) device->vtbl->unref(device);
    if (palette != NULL) palette->vtbl->unref(palette);
    sixel_allocator_unref(allocator);
    (void)pthread_mutex_unlock(&gpu_compat_gate);
    return status;
}
#else
/* Both build systems compile this file when GPU backends are disabled. */
typedef int lookup_policy_gpu_compat_disabled;
#endif
