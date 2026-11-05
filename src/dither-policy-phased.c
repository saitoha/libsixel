/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "dither-policy-phased.h"
#include "sixel_atomic.h"

typedef struct phased_preparer {
    sixel_gpu_dither_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_dither_policy_prepare_request_t configuration;
    int configured;
} phased_preparer_t;

static void
phased_ref(sixel_gpu_dither_preparer_interface_t *self)
{
    phased_preparer_t *preparer;

    preparer = (phased_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
phased_unref(sixel_gpu_dither_preparer_interface_t *self)
{
    phased_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (phased_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
phased_configure(sixel_gpu_dither_preparer_interface_t *self,
    sixel_dither_policy_prepare_request_t const *request)
{
    phased_preparer_t *preparer;

    if (request == NULL ||
            request->options.method_for_diffuse != SIXEL_DIFFUSE_PHASED ||
            request->options.ncolors < 1 || request->options.ncolors > 256 ||
            (request->options.phased_gain != SIXEL_PHASED_GAIN_STANDARD &&
             request->options.phased_gain != SIXEL_PHASED_GAIN_SMALL_ERROR)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            request->method_for_scan != SIXEL_SCAN_SERPENTINE) {
        return SIXEL_FEATURE_ERROR;
    }
    preparer = (phased_preparer_t *)self;
    preparer->configuration = *request;
    preparer->configured = 1;
    return SIXEL_OK;
}

static SIXELSTATUS
phased_configuration(sixel_gpu_dither_preparer_interface_t const *self,
    sixel_dither_policy_prepare_request_t *configuration)
{
    phased_preparer_t const *preparer;

    if (configuration == NULL) return SIXEL_BAD_ARGUMENT;
    preparer = (phased_preparer_t const *)self;
    if (!preparer->configured) return SIXEL_LOGIC_ERROR;
    *configuration = preparer->configuration;
    return SIXEL_OK;
}

static SIXELSTATUS
phased_prepare(sixel_gpu_dither_preparer_interface_t const *self,
    sixel_gpu_dither_prepare_request_t const *request,
    sixel_gpu_dither_plan_interface_t **result)
{
    phased_preparer_t const *preparer;

    if (result == NULL || request == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    preparer = (phased_preparer_t const *)self;
    if (!preparer->configured) return SIXEL_LOGIC_ERROR;
#if defined(HAVE_METAL)
    return sixel_phased_metal_prepare(preparer->allocator,
        &preparer->configuration, request, result);
#else
    return SIXEL_FEATURE_ERROR;
#endif
}

static sixel_gpu_dither_preparer_vtbl_t const phased_vtbl = {
    phased_ref, phased_unref, phased_configure,
    phased_configuration, phased_prepare
};

/* Ownership escapes through the returned component interface. */
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_dither_policy_phased_gpu_new(sixel_allocator_t *allocator, void **result)
{
    phased_preparer_t *preparer;

    if (allocator == NULL || result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    preparer = sixel_allocator_calloc(allocator, 1, sizeof(*preparer));
    if (preparer == NULL) return SIXEL_BAD_ALLOCATION;
    preparer->iface.vtbl = &phased_vtbl;
    preparer->ref = 1;
    preparer->allocator = allocator;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif
