/*
 * SPDX-License-Identifier: MIT
 *
 * Verify K-center does not flatten a large input into frame width metadata.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/dither.h"

#define KCENTER_ALLOCATION_LIMIT ((size_t)8u * 1024u * 1024u)

static int kcenter_oversize_allocation_requested;

static void *
kcenter_bounded_malloc(size_t size)
{
    if (size > KCENTER_ALLOCATION_LIMIT) {
        kcenter_oversize_allocation_requested = 1;
        return NULL;
    }
    return malloc(size);
}

static void *
kcenter_bounded_calloc(size_t count, size_t size)
{
    if (size != 0u && count > KCENTER_ALLOCATION_LIMIT / size) {
        kcenter_oversize_allocation_requested = 1;
        return NULL;
    }
    return calloc(count, size);
}

static void *
kcenter_bounded_realloc(void *ptr, size_t size)
{
    if (size > KCENTER_ALLOCATION_LIMIT) {
        kcenter_oversize_allocation_requested = 1;
        return NULL;
    }
    return realloc(ptr, size);
}

static void
kcenter_bounded_free(void *ptr)
{
    free(ptr);
}

int
test_palette_0042_kcenter_large_buffer(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_palette_entries_view_t view;
    unsigned char *pixels;
    size_t payload_size;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    dither = NULL;
    memset(&view, 0, sizeof(view));
    pixels = NULL;
    payload_size = (size_t)1024u * 1024u * 3u;
    kcenter_oversize_allocation_requested = 0;

    pixels = (unsigned char *)malloc(payload_size);
    if (pixels == NULL) {
        goto cleanup;
    }
    memset(pixels, 96, payload_size);
    status = sixel_allocator_new(&allocator,
                                 kcenter_bounded_malloc,
                                 kcenter_bounded_calloc,
                                 kcenter_bounded_realloc,
                                 kcenter_bounded_free);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_dither_new(&dither, 4, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    dither->quantize_model = SIXEL_QUANTIZE_MODEL_KCENTER;
    dither->final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    status = sixel_dither_initialize(
        dither,
        pixels,
        1024,
        1024,
        SIXEL_PIXELFORMAT_RGB888,
        SIXEL_LARGE_AUTO,
        SIXEL_REP_AUTO,
        SIXEL_QUALITY_AUTO);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = dither->palette->vtbl->get_entries(dither->palette, &view);
    if (SIXEL_FAILED(status) || view.entry_count != 1u ||
            view.depth != 3 || view.entries == NULL ||
            view.entries[0] != 96u || view.entries[1] != 96u ||
            view.entries[2] != 96u ||
            kcenter_oversize_allocation_requested != 0) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = SIXEL_OK;

cleanup:
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    free(pixels);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "K-center large-buffer regression failed\n");
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
