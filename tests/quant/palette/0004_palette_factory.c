/*
 * SPDX-License-Identifier: MIT
 *
 * Unit test for palette creation through the component factory.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

#include "src/factory.h"

/*
 * IDL usage in this unit
 *
 * IComponents.getservice("services/factory", &factory)
 * IFactory.create("quant/palette", allocator, &palette)
 * IPalette.init_entries(request)
 * IPalette.init_entries_float32(request)
 * IPalette.get_entries(view)
 * IPalette.get_entries_float32(view)
 * IPalette.get_metadata(metadata)
 * IPalette.ref()
 * IPalette.unref()
 */

static int palette_tracked_allocator_free_count;

static void *
palette_tracked_malloc(size_t size)
{
    return malloc(size);
}

static void *
palette_tracked_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

static void *
palette_tracked_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static void
palette_tracked_free(void *ptr)
{
    if (ptr != NULL) {
        ++palette_tracked_allocator_free_count;
    }
    free(ptr);
}

static int
test_palette_factory_component(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_palette_t *palette;
    sixel_palette_entries_request_t entries_request;
    sixel_palette_float32_entries_request_t float32_request;
    sixel_palette_entries_view_t entries_view;
    sixel_palette_float32_entries_view_t float32_view;
    sixel_palette_metadata_t metadata;
    void *service;
    void *object;
    unsigned char entries[6];
    float entries_float32[6];
    int free_count_before_unref;

    status = SIXEL_FALSE;
    allocator = NULL;
    factory = NULL;
    palette = NULL;
    service = NULL;
    object = NULL;
    free_count_before_unref = 0;
    memset(&entries_request, 0, sizeof(entries_request));
    memset(&float32_request, 0, sizeof(float32_request));
    memset(&entries_view, 0, sizeof(entries_view));
    memset(&float32_view, 0, sizeof(float32_view));
    memset(&metadata, 0, sizeof(metadata));
    entries[0] = 0x00;
    entries[1] = 0x11;
    entries[2] = 0x22;
    entries[3] = 0xaa;
    entries[4] = 0xbb;
    entries[5] = 0xcc;
    entries_float32[0] = 0.0f;
    entries_float32[1] = 0.1f;
    entries_float32[2] = 0.2f;
    entries_float32[3] = 0.8f;
    entries_float32[4] = 0.9f;
    entries_float32[5] = 1.0f;

    palette_tracked_allocator_free_count = 0;
    status = sixel_allocator_new(&allocator,
                                 palette_tracked_malloc,
                                 palette_tracked_calloc,
                                 palette_tracked_realloc,
                                 palette_tracked_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    factory = (sixel_factory_t *)service;
    if (factory == NULL || factory->vtbl == NULL ||
        factory->vtbl->create == NULL ||
        factory->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = factory->vtbl->create(factory, "quant/palette",
                                   allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    palette = (sixel_palette_t *)object;
    object = NULL;
    if (palette == NULL || palette->vtbl == NULL ||
        palette->vtbl->ref == NULL ||
        palette->vtbl->unref == NULL ||
        palette->vtbl->init_entries == NULL ||
        palette->vtbl->init_entries_float32 == NULL ||
        palette->vtbl->get_entries == NULL ||
        palette->vtbl->get_entries_float32 == NULL ||
        palette->vtbl->get_metadata == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    /*
     * The palette retains the allocator internally. Dropping the caller's
     * reference keeps allocator lifetime ownership hidden behind the component.
     */
    sixel_allocator_unref(allocator);
    allocator = NULL;

    entries_request.entries = entries;
    entries_request.colors = 2U;
    entries_request.depth = 3;
    status = palette->vtbl->init_entries(palette, &entries_request);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = palette->vtbl->get_entries(palette, &entries_view);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (entries_view.entries == NULL ||
        entries_view.entry_count != 2U ||
        entries_view.depth != 3 ||
        entries_view.entries_size < sizeof(entries) ||
        memcmp(entries_view.entries, entries, sizeof(entries)) != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    float32_request.entries = entries_float32;
    float32_request.colors = 2U;
    float32_request.depth = 3 * (int)sizeof(float);
    status = palette->vtbl->init_entries_float32(palette,
                                                 &float32_request);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = palette->vtbl->get_entries_float32(palette, &float32_view);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (float32_view.entries == NULL ||
        float32_view.entry_count != 2U ||
        float32_view.depth != 3 * (int)sizeof(float) ||
        float32_view.entries_size < sizeof(entries_float32) ||
        memcmp(float32_view.entries,
               entries_float32,
               sizeof(entries_float32)) != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = palette->vtbl->get_metadata(palette, &metadata);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (metadata.entry_count != 2U ||
        metadata.requested_colors != 2U ||
        metadata.depth != 3 ||
        metadata.float_depth != 3 * (int)sizeof(float)) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    palette->vtbl->ref(palette);
    palette->vtbl->unref(palette);

    free_count_before_unref = palette_tracked_allocator_free_count;
    palette->vtbl->unref(palette);
    palette = NULL;
    if (palette_tracked_allocator_free_count <= free_count_before_unref) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (object != NULL) {
        ((sixel_palette_t *)object)->vtbl->unref((sixel_palette_t *)object);
    }
    if (palette != NULL && palette->vtbl != NULL &&
        palette->vtbl->unref != NULL) {
        palette->vtbl->unref(palette);
    }
    if (factory != NULL && factory->vtbl != NULL &&
        factory->vtbl->unref != NULL) {
        factory->vtbl->unref(factory);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return SIXEL_SUCCEEDED(status);
}

int
test_palette_0004_palette_factory(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!test_palette_factory_component()) {
        fprintf(stderr, "palette factory component contract failed\n");
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
