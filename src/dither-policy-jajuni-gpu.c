/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <string.h>
#include "dither-policy-jajuni-gpu.h"
#include "sixel_atomic.h"

typedef struct jajuni_gpu_preparer {
    sixel_gpu_dither_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_dither_policy_prepare_request_t configuration;
    int configured;
} jajuni_gpu_preparer_t;

static void
jajuni_gpu_ref(sixel_gpu_dither_preparer_interface_t *self)
{
    jajuni_gpu_preparer_t *preparer;

    preparer = (jajuni_gpu_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
jajuni_gpu_unref(sixel_gpu_dither_preparer_interface_t *self)
{
    jajuni_gpu_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (jajuni_gpu_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
jajuni_gpu_configure(sixel_gpu_dither_preparer_interface_t *self,
    sixel_dither_policy_prepare_request_t const *request)
{
    jajuni_gpu_preparer_t *preparer;

    if (request == NULL ||
            request->options.method_for_diffuse != SIXEL_DIFFUSE_JAJUNI ||
            request->options.ncolors < 1 || request->options.ncolors > 256 ||
            !isfinite(request->options.clamp_threshold) ||
            request->options.clamp_threshold < 1.0 ||
            request->options.clamp_threshold > 16.0 ||
            !(request->options.diffusion_deadzone >= 0.0) ||
            request->options.clamp_policy < SIXEL_DITHER_CLAMP_AUTO ||
            request->options.clamp_policy > SIXEL_DITHER_CLAMP_NONE) {
        return SIXEL_BAD_ARGUMENT;
    }
    /* The GPU carry follows the CPU limiter contract. The unbounded and
     * perturbed carries have no GPU form; their CPU forms remain. */
    if (request->options.clamp_policy == SIXEL_DITHER_CLAMP_NONE ||
            request->options.diffusion_perturb != 0.0f ||
            request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            (request->method_for_scan != SIXEL_SCAN_SERPENTINE &&
             request->method_for_scan != SIXEL_SCAN_RASTER)) {
        return SIXEL_FEATURE_ERROR;
    }
    preparer = (jajuni_gpu_preparer_t *)self;
    preparer->configuration = *request;
    preparer->configured = 1;
    return SIXEL_OK;
}

static SIXELSTATUS
jajuni_gpu_configuration(sixel_gpu_dither_preparer_interface_t const *self,
    sixel_dither_policy_prepare_request_t *configuration)
{
    jajuni_gpu_preparer_t const *preparer;

    if (configuration == NULL) return SIXEL_BAD_ARGUMENT;
    preparer = (jajuni_gpu_preparer_t const *)self;
    if (!preparer->configured) return SIXEL_LOGIC_ERROR;
    *configuration = preparer->configuration;
    return SIXEL_OK;
}

static SIXELSTATUS
jajuni_gpu_prepare(sixel_gpu_dither_preparer_interface_t const *self,
    sixel_gpu_dither_prepare_request_t const *request,
    sixel_gpu_dither_plan_interface_t **result)
{
    jajuni_gpu_preparer_t const *preparer;

    if (result == NULL || request == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    preparer = (jajuni_gpu_preparer_t const *)self;
    if (!preparer->configured) return SIXEL_LOGIC_ERROR;
#if defined(HAVE_METAL)
    return sixel_dither_jajuni_gpu_metal_prepare(preparer->allocator,
        &preparer->configuration, request, result);
#elif defined(HAVE_VULKAN)
    return sixel_dither_jajuni_gpu_vulkan_prepare(preparer->allocator,
        &preparer->configuration, request, result);
#else
    return SIXEL_FEATURE_ERROR;
#endif
}

static sixel_gpu_dither_preparer_vtbl_t const jajuni_gpu_vtbl = {
    jajuni_gpu_ref, jajuni_gpu_unref, jajuni_gpu_configure,
    jajuni_gpu_configuration, jajuni_gpu_prepare
};

/* Ownership escapes through the returned component interface. */
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_dither_policy_jajuni_gpu_new(sixel_allocator_t *allocator, void **result)
{
    jajuni_gpu_preparer_t *preparer;

    if (allocator == NULL || result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    preparer = sixel_allocator_calloc(allocator, 1, sizeof(*preparer));
    if (preparer == NULL) return SIXEL_BAD_ALLOCATION;
    preparer->iface.vtbl = &jajuni_gpu_vtbl;
    preparer->ref = 1;
    preparer->allocator = allocator;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif
