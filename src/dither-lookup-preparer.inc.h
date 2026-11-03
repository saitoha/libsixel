/* SPDX-License-Identifier: MIT */

/* The compatibility root freezes old option fields once. The CPU provider
 * builds lazily and the returned wrapper owns the byte palette even for old
 * policies that borrow their prepare input rather than copying it. */
typedef struct dither_lookup_preparer {
    sixel_lookup_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_dither_t snapshot;
    sixel_dither_t *owner;
    sixel_lookup_policy_interface_t *reuse_owner;
} dither_lookup_preparer_t;

typedef struct frozen_lookup {
    sixel_lookup_policy_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_lookup_policy_interface_t *lookup;
    unsigned char palette[768];
} frozen_lookup_t;

static void
frozen_lookup_ref(sixel_lookup_policy_interface_t *self)
{
    frozen_lookup_t *lookup;

    lookup = (frozen_lookup_t *)self;
    (void)sixel_atomic_fetch_add_u32(&lookup->ref, 1);
}

static void
frozen_lookup_unref(sixel_lookup_policy_interface_t *self)
{
    frozen_lookup_t *lookup;
    sixel_allocator_t *allocator;

    lookup = (frozen_lookup_t *)self;
    if (sixel_atomic_fetch_sub_u32(&lookup->ref, 1) != 1) return;
    allocator = lookup->allocator;
    if (lookup->lookup != NULL) lookup->lookup->vtbl->unref(lookup->lookup);
    sixel_allocator_free(allocator, lookup);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
frozen_lookup_prepare(sixel_lookup_policy_interface_t *self,
    sixel_lookup_policy_prepare_request_t const *request)
{
    (void)self;
    (void)request;
    return SIXEL_LOGIC_ERROR;
}

static sixel_lookup_policy_result_t
frozen_lookup_pixel(sixel_lookup_policy_interface_t const *self,
    unsigned char const *pixel)
{
    frozen_lookup_t const *lookup;

    lookup = (frozen_lookup_t const *)self;
    return lookup->lookup->vtbl->map_pixel(lookup->lookup, pixel);
}

static sixel_lookup_policy_result_t
frozen_lookup_hint(sixel_lookup_policy_interface_t const *self,
    unsigned char const *pixel, int hint)
{
    frozen_lookup_t const *lookup;

    lookup = (frozen_lookup_t const *)self;
    if (lookup->lookup->vtbl->map_pixel_hint == NULL) {
        return lookup->lookup->vtbl->map_pixel(lookup->lookup, pixel);
    }
    return lookup->lookup->vtbl->map_pixel_hint(lookup->lookup, pixel, hint);
}

static void
frozen_lookup_pixels(sixel_lookup_policy_interface_t const *self,
    unsigned char const *pixels, size_t count, int stride,
    sixel_index_t *result, int result_stride, void *workspace)
{
    frozen_lookup_t const *lookup;
    size_t i;

    lookup = (frozen_lookup_t const *)self;
    if (lookup->lookup->vtbl->map_pixels != NULL) {
        lookup->lookup->vtbl->map_pixels(lookup->lookup, pixels, count, stride,
            result, result_stride, workspace);
    } else {
        for (i = 0; i < count; ++i) {
            *result = (sixel_index_t)lookup->lookup->vtbl->map_pixel(
                lookup->lookup, pixels);
            pixels += stride;
            result += result_stride;
        }
    }
}

static size_t
frozen_lookup_workspace(sixel_lookup_policy_interface_t const *self)
{
    frozen_lookup_t const *lookup;

    lookup = (frozen_lookup_t const *)self;
    return lookup->lookup->vtbl->workspace_size == NULL ? 0
        : lookup->lookup->vtbl->workspace_size(lookup->lookup);
}

static int
frozen_lookup_kind(sixel_lookup_policy_interface_t const *self)
{
    frozen_lookup_t const *lookup;

    lookup = (frozen_lookup_t const *)self;
    return lookup->lookup->vtbl->get_kind(lookup->lookup);
}

static SIXELSTATUS
frozen_lookup_configuration(sixel_lookup_policy_interface_t const *self,
    sixel_lookup_policy_configuration_t *configuration)
{
    frozen_lookup_t const *lookup;

    lookup = (frozen_lookup_t const *)self;
    if (lookup->lookup->vtbl->get_configuration == NULL) {
        return SIXEL_FEATURE_ERROR;
    }
    return lookup->lookup->vtbl->get_configuration(lookup->lookup,
        configuration);
}

static sixel_lookup_policy_vtbl_t const frozen_lookup_vtbl = {
    frozen_lookup_ref, frozen_lookup_unref, frozen_lookup_prepare,
    frozen_lookup_pixel, frozen_lookup_hint, frozen_lookup_pixels,
    frozen_lookup_workspace, frozen_lookup_kind, frozen_lookup_configuration
};

static void
dither_lookup_ref(sixel_lookup_preparer_interface_t *self)
{
    dither_lookup_preparer_t *preparer;

    preparer = (dither_lookup_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
dither_lookup_unref(sixel_lookup_preparer_interface_t *self)
{
    dither_lookup_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (dither_lookup_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->snapshot.allocator;
    sixel_dither_unref(preparer->owner);
    if (preparer->reuse_owner != NULL) {
        preparer->reuse_owner->vtbl->unref(preparer->reuse_owner);
    }
    sixel_timeline_logger_unref(preparer->snapshot.pipeline_logger);
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
dither_lookup_prepare(sixel_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_lookup_policy_interface_t **result)
{
    dither_lookup_preparer_t const *preparer;
    frozen_lookup_t *lookup;
    sixel_palette_entries_view_t entries;
    sixel_dither_t const *snapshot;
    sixel_lookup_policy_interface_t *reuse;
    SIXELSTATUS status;
    int shared;

    if (request == NULL || request->working_palette == NULL || result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            request->colorspace != SIXEL_COLORSPACE_GAMMA) {
        return SIXEL_FEATURE_ERROR;
    }
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
    preparer = (dither_lookup_preparer_t const *)self;
    snapshot = &preparer->snapshot;
    lookup = sixel_allocator_calloc(snapshot->allocator, 1, sizeof(*lookup));
    if (lookup == NULL) return SIXEL_BAD_ALLOCATION;
    lookup->iface.vtbl = &frozen_lookup_vtbl;
    lookup->ref = 1U;
    lookup->allocator = snapshot->allocator;
    sixel_allocator_ref(lookup->allocator);
    memcpy(lookup->palette, entries.entries, request->ncolors * 3U);
    shared = sixel_dither_lookup_shared_instance_enabled(snapshot,
        snapshot->lut_policy);
    /* FHEDT's legacy reuse operation retains an immutable grid. Other old
     * policies can transfer ownership destructively, so this compatibility
     * adapter must not offer them a plan that may still have readers. */
    reuse = preparer->reuse_owner;
    if (reuse != NULL && reuse->vtbl == &frozen_lookup_vtbl) {
        reuse = ((frozen_lookup_t *)reuse)->lookup;
    }
    status = sixel_dither_prepare_lookup_policy(&lookup->lookup,
        lookup->palette, NULL, 3, 0, (int)request->ncolors,
        snapshot->optimized, snapshot->lut_policy, shared, snapshot,
        request->pixelformat, request->parallel_dither_active, reuse, NULL,
        snapshot->allocator);
    if (SIXEL_FAILED(status)) {
        frozen_lookup_unref(&lookup->iface);
        return status;
    }
    /* Preserve the legacy root's prepared-lookup observation without letting
     * later option changes mutate this frozen lookup. */
    if (preparer->owner->lookup_policy != NULL) {
        preparer->owner->lookup_policy->vtbl->unref(
            preparer->owner->lookup_policy);
    }
    preparer->owner->lookup_policy = &lookup->iface;
    frozen_lookup_ref(&lookup->iface);
    *result = &lookup->iface;
    return SIXEL_OK;
}

static sixel_lookup_preparer_vtbl_t const dither_lookup_vtbl = {
    dither_lookup_ref, dither_lookup_unref, dither_lookup_prepare
};

SIXELSTATUS
sixel_dither_lookup_preparers(sixel_dither_t *dither,
    sixel_palette_apply_prepare_request_t *request)
{
    dither_lookup_preparer_t *cpu;
    sixel_lookup_policy_configuration_t configuration;
    sixel_gpu_lookup_preparer_interface_t *gpu;
    SIXELSTATUS status;

    cpu = sixel_allocator_calloc(dither->allocator, 1, sizeof(*cpu));
    if (cpu == NULL) return SIXEL_BAD_ALLOCATION;
    cpu->iface.vtbl = &dither_lookup_vtbl;
    cpu->ref = 1U;
    /* Only scalar option fields are consumed during deferred preparation.
     * Retain diagnostics explicitly; palette and lookup are not borrowed. */
    cpu->snapshot = *dither;
    cpu->owner = dither;
    if (dither->lut_policy == SIXEL_LUT_POLICY_FHEDT &&
            dither->lookup_policy != NULL) {
        cpu->reuse_owner = dither->lookup_policy;
        cpu->reuse_owner->vtbl->ref(cpu->reuse_owner);
    }
    sixel_dither_ref(dither);
    if (cpu->snapshot.pipeline_logger != NULL) {
        cpu->snapshot.pipeline_logger->vtbl->ref(cpu->snapshot.pipeline_logger);
    }
    cpu->snapshot.palette = NULL;
    cpu->snapshot.lookup_policy = NULL;
    sixel_allocator_ref(dither->allocator);
    request->cpu_lookup = &cpu->iface;
    if (request->gpu_policy == SIXEL_GPU_POLICY_OFF) return SIXEL_OK;
    status = sixel_dither_get_lookup_configuration(dither,
        request->pixelformat, &configuration);
    if (SIXEL_FAILED(status)) return status;
    if (dither->gpu_lookup == NULL || memcmp(&configuration,
            &dither->gpu_lookup_configuration, sizeof(configuration)) != 0) {
        gpu = NULL;
        status = sixel_gpu_lookup_select(dither->allocator, &configuration,
            &gpu);
        if (status == SIXEL_FEATURE_ERROR) return SIXEL_OK;
        if (SIXEL_FAILED(status)) return status;
        if (dither->gpu_lookup != NULL) {
            dither->gpu_lookup->vtbl->unref(dither->gpu_lookup);
        }
        dither->gpu_lookup = gpu;
        dither->gpu_lookup_configuration = configuration;
    }
    request->gpu_lookup = dither->gpu_lookup;
    request->gpu_lookup->vtbl->ref(request->gpu_lookup);
    status = sixel_gpu_device_acquire(&request->device);
    return status == SIXEL_FEATURE_ERROR ? SIXEL_OK : status;
}
