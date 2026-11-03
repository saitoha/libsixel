/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "gpu-lookup-legacy.h"
#include "threading.h"
#include "sixel_atomic.h"
#include "gpu-palette.h"

/* Explicit migration adapter for the pre-component GPU implementations.
 * It freezes old settings outside PaletteApply. New lookup components never
 * enter this adapter or extend the legacy shader's policy switches. */
typedef struct legacy_plan {
    sixel_gpu_lookup_plan_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *workspace;
    sixel_mutex_t gate;
    unsigned char palette[768];
    unsigned int count;
    int busy;
    int valid;
    sixel_lookup_policy_configuration_t configuration;
} legacy_plan_t;

typedef struct legacy_binding {
    sixel_gpu_lookup_binding_interface_t iface;
    sixel_atomic_u32_t ref;
    legacy_plan_t *plan;
    int finished;
    int rebuild;
} legacy_binding_t;

typedef struct legacy_preparer {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_mutex_t gate;
    legacy_plan_t *cached;
    sixel_lookup_policy_configuration_t configuration;
} legacy_preparer_t;

static void
legacy_plan_ref(sixel_gpu_lookup_plan_interface_t *self)
{
    legacy_plan_t *plan;

    plan = (legacy_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
legacy_plan_unref(sixel_gpu_lookup_plan_interface_t *self)
{
    legacy_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (legacy_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) return;
    allocator = plan->allocator;
    if (plan->workspace != NULL) plan->workspace->vtbl->unref(plan->workspace);
    plan->device->vtbl->unref(plan->device);
    sixel_mutex_destroy(&plan->gate);
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
legacy_describe(sixel_gpu_lookup_plan_interface_t const *self,
    sixel_gpu_lookup_program_view_t *view)
{
    legacy_plan_t const *plan;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    plan = (legacy_plan_t const *)self;
    memset(view, 0, sizeof(*view));
    view->program_format = "libsixel.legacy-palette.v1";
    view->logical_name = "legacy-adapter";
    view->palette = plan->palette;
    view->palette_size = plan->count * 3U;
    view->ncolors = plan->count;
    view->constants = &plan->configuration;
    view->constants_size = sizeof(plan->configuration);
    view->workspace_size = 1;
    return SIXEL_OK;
}

static void
legacy_binding_ref(sixel_gpu_lookup_binding_interface_t *self)
{
    legacy_binding_t *binding;

    binding = (legacy_binding_t *)self;
    (void)sixel_atomic_fetch_add_u32(&binding->ref, 1);
}

static void
legacy_binding_unref(sixel_gpu_lookup_binding_interface_t *self)
{
    legacy_binding_t *binding;
    legacy_plan_t *plan;

    binding = (legacy_binding_t *)self;
    if (sixel_atomic_fetch_sub_u32(&binding->ref, 1) != 1) return;
    plan = binding->plan;
    /* An unfinished lease intentionally remains busy; unref cannot certify
     * GPU completion or allow another invocation to overwrite its storage. */
    sixel_allocator_free(plan->allocator, binding);
    legacy_plan_unref(&plan->iface);
}

static SIXELSTATUS
legacy_binding_view(sixel_gpu_lookup_binding_interface_t const *self,
    sixel_gpu_lookup_binding_view_t *view)
{
    legacy_binding_t const *binding;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    binding = (legacy_binding_t const *)self;
    sixel_mutex_lock(&binding->plan->gate);
    if (binding->finished) {
        sixel_mutex_unlock(&binding->plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    view->workspace = binding->plan->workspace;
    view->needs_build = binding->rebuild;
    sixel_mutex_unlock(&binding->plan->gate);
    return SIXEL_OK;
}

static SIXELSTATUS
legacy_finish(sixel_gpu_lookup_binding_interface_t *self, SIXELSTATUS outcome)
{
    legacy_binding_t *binding;
    legacy_plan_t *plan;

    binding = (legacy_binding_t *)self;
    plan = binding->plan;
    sixel_mutex_lock(&plan->gate);
    if (binding->finished) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    binding->finished = 1;
    if (binding->rebuild) plan->valid = outcome == SIXEL_OK;
    plan->busy = 0;
    sixel_mutex_unlock(&plan->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_binding_vtbl_t const legacy_binding_vtbl = {
    legacy_binding_ref, legacy_binding_unref, legacy_binding_view, legacy_finish
};

static SIXELSTATUS
legacy_begin(sixel_gpu_lookup_plan_interface_t *self,
    sixel_gpu_lookup_binding_interface_t **result)
{
    legacy_plan_t *plan;
    legacy_binding_t *binding;
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    plan = (legacy_plan_t *)self;
    sixel_mutex_lock(&plan->gate);
    if (plan->busy) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    status = SIXEL_OK;
    if (plan->workspace == NULL) {
        status = plan->device->vtbl->create_buffer(plan->device,
            1, &plan->workspace);
    }
    if (SIXEL_FAILED(status)) {
        sixel_mutex_unlock(&plan->gate);
        return status;
    }
    binding = sixel_allocator_calloc(plan->allocator, 1, sizeof(*binding));
    if (binding == NULL) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_BAD_ALLOCATION;
    }
    binding->iface.vtbl = &legacy_binding_vtbl;
    binding->ref = 1U;
    binding->plan = plan;
    binding->rebuild = !plan->valid;
    legacy_plan_ref(self);
    plan->busy = 1;
    sixel_mutex_unlock(&plan->gate);
    *result = &binding->iface;
    return SIXEL_OK;
}

static sixel_gpu_lookup_plan_vtbl_t const legacy_plan_vtbl = {
    legacy_plan_ref, legacy_plan_unref, legacy_describe, legacy_begin
};

static void
legacy_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    legacy_preparer_t *preparer;

    preparer = (legacy_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
legacy_unref(sixel_gpu_lookup_preparer_interface_t *self)
{
    legacy_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (legacy_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    if (preparer->cached != NULL) legacy_plan_unref(&preparer->cached->iface);
    sixel_mutex_destroy(&preparer->gate);
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
legacy_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    legacy_preparer_t *preparer;
    legacy_plan_t *plan;
    sixel_palette_entries_view_t entries;
    sixel_gpu_palette_request_t admission;
    SIXELSTATUS status;

    if (request == NULL || device == NULL || result == NULL ||
            request->working_palette == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            request->colorspace != SIXEL_COLORSPACE_GAMMA ||
            strcmp(device->vtbl->program_format(device),
                "metal.lookup-rgb8.v1") != 0) return SIXEL_FEATURE_ERROR;
    if (request->ncolors < 1 || request->ncolors > 256) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = request->working_palette->vtbl->get_entries(
        request->working_palette, &entries);
    if (SIXEL_FAILED(status)) return status;
    if (entries.depth != 3 || entries.entries == NULL ||
            entries.entry_count < request->ncolors ||
            entries.entries_size < request->ncolors * 3U) {
        return SIXEL_BAD_ARGUMENT;
    }
    preparer = (legacy_preparer_t *)self;
    memset(&admission, 0, sizeof(admission));
    sixel_gpu_palette_set_lookup_configuration(&admission,
        &preparer->configuration);
    if (preparer->configuration.kind == SIXEL_LUT_POLICY_FHEDT &&
            !sixel_gpu_palette_fhedt_is_supported(&admission)) {
        return SIXEL_FEATURE_ERROR;
    }
    sixel_mutex_lock(&preparer->gate);
    plan = preparer->cached;
    if (plan == NULL || plan->device != device ||
            plan->count != request->ncolors ||
            memcmp(plan->palette, entries.entries, request->ncolors * 3U)) {
        plan = sixel_allocator_calloc(preparer->allocator, 1, sizeof(*plan));
        if (plan == NULL) {
            sixel_mutex_unlock(&preparer->gate);
            return SIXEL_BAD_ALLOCATION;
        }
#if SIXEL_ENABLE_THREADS
        status = sixel_mutex_init(&plan->gate);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(preparer->allocator, plan);
            sixel_mutex_unlock(&preparer->gate);
            return status;
        }
#endif
        plan->iface.vtbl = &legacy_plan_vtbl;
        plan->ref = 1U;
        plan->allocator = preparer->allocator;
        sixel_allocator_ref(plan->allocator);
        plan->device = device;
        device->vtbl->ref(device);
        plan->count = request->ncolors;
        memcpy(plan->palette, entries.entries, plan->count * 3U);
        plan->configuration = preparer->configuration;
        if (preparer->cached != NULL) {
            legacy_plan_unref(&preparer->cached->iface);
        }
        preparer->cached = plan;
    }
    legacy_plan_ref(&plan->iface);
    *result = &plan->iface;
    sixel_mutex_unlock(&preparer->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_preparer_vtbl_t const legacy_vtbl = {
    legacy_ref, legacy_unref, legacy_prepare
};

SIXELSTATUS
sixel_gpu_lookup_legacy_create(sixel_allocator_t *allocator,
    sixel_lookup_policy_configuration_t const *configuration,
    sixel_gpu_lookup_preparer_interface_t **result)
{
    legacy_preparer_t *preparer;
#if SIXEL_ENABLE_THREADS
    SIXELSTATUS status;
#endif

    if (allocator == NULL || result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    preparer = sixel_allocator_calloc(allocator, 1, sizeof(*preparer));
    if (preparer == NULL) return SIXEL_BAD_ALLOCATION;
#if SIXEL_ENABLE_THREADS
    status = sixel_mutex_init(&preparer->gate);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, preparer);
        return status;
    }
#endif
    preparer->iface.vtbl = &legacy_vtbl;
    preparer->ref = 1U;
    preparer->allocator = allocator;
    preparer->configuration = *configuration;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
