/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 libsixel developers
 */

#include "config.h"

#include <stdlib.h>

#include "palette.h"
#include "allocator.h"
#include "quant.h"
#include "status.h"
#include "quant-internal.h"

#if HAVE_STRING_H
# include <string.h>
#endif

static void
sixel_palette_dispose(sixel_palette_t *palette);

/*
 * Resize the palette entry buffer.
 *
 * The helper keeps the allocation logic in a single place so both k-means and
 * median-cut paths can rely on the same growth strategy.  When the caller
 * requests a size of zero the buffer is released entirely.
 */
static SIXELSTATUS
sixel_palette_resize_entries(sixel_palette_t *palette,
                             unsigned int colors,
                             unsigned int depth,
                             sixel_allocator_t *allocator);

SIXELAPI SIXELSTATUS
sixel_palette_new(sixel_palette_t **palette, sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_palette_t *object;

    if (palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_new: palette pointer is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            *palette = NULL;
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    object = (sixel_palette_t *)sixel_allocator_malloc(
        allocator, sizeof(*object));
    if (object == NULL) {
        sixel_allocator_unref(allocator);
        sixel_helper_set_additional_message(
            "sixel_palette_new: allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        *palette = NULL;
        goto end;
    }

    object->ref = 1U;
    object->allocator = allocator;
    object->entries = NULL;
    object->entries_size = 0U;
    object->entry_count = 0U;
    object->requested_colors = 0U;
    object->original_colors = 0U;
    object->depth = 0;
    object->method_for_largest = SIXEL_LARGE_AUTO;
    object->method_for_rep = SIXEL_REP_AUTO;
    object->quality_mode = SIXEL_QUALITY_AUTO;
    object->force_palette = 0;
    object->use_reversible = 0;
    object->quantize_model = SIXEL_QUANTIZE_MODEL_AUTO;
    object->final_merge_mode = SIXEL_FINAL_MERGE_AUTO;
    object->complexion = 1;
    object->lut_policy = SIXEL_LUT_POLICY_AUTO;
    object->sixel_reversible = 0;
    object->final_merge = 0;
    object->cachetable = NULL;
    object->cachetable_size = 0U;

    *palette = object;
    status = SIXEL_OK;

end:
    return status;
}

SIXELAPI sixel_palette_t *
sixel_palette_ref(sixel_palette_t *palette)
{
    if (palette != NULL) {
        ++palette->ref;
    }

    return palette;
}

static void
sixel_palette_dispose(sixel_palette_t *palette)
{
    sixel_allocator_t *allocator;

    if (palette == NULL) {
        return;
    }

    allocator = palette->allocator;
    if (palette->entries != NULL) {
        sixel_allocator_free(allocator, palette->entries);
        palette->entries = NULL;
    }

    if (palette->cachetable != NULL) {
        sixel_quant_cache_release(palette->cachetable,
                                  palette->lut_policy,
                                  allocator);
        palette->cachetable = NULL;
        palette->cachetable_size = 0U;
    }

    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
}

SIXELAPI void
sixel_palette_unref(sixel_palette_t *palette)
{
    if (palette == NULL) {
        return;
    }

    if (palette->ref > 1U) {
        --palette->ref;
        return;
    }

    sixel_palette_dispose(palette);
    sixel_allocator_free(palette->allocator, palette);
}

static SIXELSTATUS
sixel_palette_resize_entries(sixel_palette_t *palette,
                             unsigned int colors,
                             unsigned int depth,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t required;
    unsigned char *resized;

    if (palette == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    required = (size_t)colors * (size_t)depth;
    if (required == 0U) {
        if (palette->entries != NULL) {
            sixel_allocator_free(allocator, palette->entries);
            palette->entries = NULL;
        }
        palette->entries_size = 0U;
        return SIXEL_OK;
    }

    if (palette->entries != NULL && palette->entries_size >= required) {
        return SIXEL_OK;
    }

    if (palette->entries == NULL) {
        resized = (unsigned char *)sixel_allocator_malloc(allocator, required);
    } else {
        resized = (unsigned char *)sixel_allocator_realloc(allocator,
                                                           palette->entries,
                                                           required);
    }
    if (resized == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_resize_entries: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    palette->entries = resized;
    palette->entries_size = required;

    status = SIXEL_OK;

    return status;
}

SIXELAPI SIXELSTATUS
sixel_palette_resize(sixel_palette_t *palette,
                     unsigned int colors,
                     int depth,
                     sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *work_allocator;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (depth < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_palette_resize_entries(palette,
                                          colors,
                                          (unsigned int)depth,
                                          work_allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    palette->entry_count = colors;
    palette->depth = depth;

    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_palette_set_entries(sixel_palette_t *palette,
                          unsigned char const *entries,
                          unsigned int colors,
                          int depth,
                          sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t payload_size;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_palette_resize(palette, colors, depth, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    payload_size = (size_t)colors * (size_t)depth;
    if (entries != NULL && palette->entries != NULL && payload_size > 0U) {
        memcpy(palette->entries, entries, payload_size);
    }

    return SIXEL_OK;
}

SIXELAPI unsigned char *
sixel_palette_get_entries(sixel_palette_t *palette)
{
    if (palette == NULL) {
        return NULL;
    }

    return palette->entries;
}

SIXELAPI unsigned int
sixel_palette_get_entry_count(sixel_palette_t const *palette)
{
    if (palette == NULL) {
        return 0U;
    }

    return palette->entry_count;
}

SIXELAPI int
sixel_palette_get_depth(sixel_palette_t const *palette)
{
    if (palette == NULL) {
        return 0;
    }

    return palette->depth;
}

/*
 * sixel_palette_generate builds the palette entries inside the provided
 * sixel_palette_t instance.  The procedure performs two major steps:
 *
 *   1. Try the k-means implementation when explicitly requested.  Successful
 *      runs populate a temporary buffer that is copied into the palette.
 *   2. Fall back to the existing histogram and median-cut pipeline.  The
 *      resulting tuple table is converted into tightly packed palette bytes.
 *
 * Both branches share helper routines for cache management and post-processing
 * (for example reversible palette transformation).  The palette object tracks
 * the generated metadata so the caller can publish it without recomputing.
 */
SIXELSTATUS
sixel_palette_generate(sixel_palette_t *palette,
                       unsigned char const *data,
                       unsigned int length,
                       int pixelformat,
                       sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    tupletable2 colormap = { 0U, NULL };
    unsigned char *kmeans_entries = NULL;
    unsigned int ncolors = 0U;
    unsigned int origcolors = 0U;
    unsigned int depth = 0U;
    int result_depth;
    sixel_allocator_t *work_allocator;
    size_t payload_size;
    int ret;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    result_depth = sixel_helper_compute_depth(pixelformat);
    if (result_depth <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_generate: invalid pixel format depth.");
        return SIXEL_BAD_ARGUMENT;
    }
    depth = (unsigned int)result_depth;

    status = SIXEL_FALSE;
    payload_size = 0U;

    if (palette->quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
        status = build_palette_kmeans(&kmeans_entries,
                                      data,
                                      length,
                                      depth,
                                      palette->requested_colors,
                                      &ncolors,
                                      &origcolors,
                                      palette->quality_mode,
                                      palette->force_palette,
                                      palette->final_merge_mode,
                                      work_allocator);
        if (SIXEL_SUCCEEDED(status)) {
            if (palette->use_reversible) {
                sixel_quant_reversible_palette(kmeans_entries,
                                               ncolors,
                                               depth);
            }
            payload_size = (size_t)ncolors * (size_t)depth;
            status = sixel_palette_resize_entries(palette,
                                                  ncolors,
                                                  depth,
                                                  work_allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            if (payload_size > 0U) {
                memcpy(palette->entries, kmeans_entries, payload_size);
            }
            status = SIXEL_OK;
            goto success;
        }
    }

    ret = computeColorMapFromInput(data,
                                   length,
                                   depth,
                                   palette->requested_colors,
                                   palette->method_for_largest,
                                   palette->method_for_rep,
                                   palette->quality_mode,
                                   palette->force_palette,
                                   palette->use_reversible,
                                   palette->final_merge_mode,
                                   &colormap,
                                   &origcolors,
                                   work_allocator);
    if (ret != 0) {
        status = SIXEL_FALSE;
        sixel_helper_set_additional_message(
            "sixel_palette_generate: color map construction failed.");
        goto end;
    }

    ncolors = colormap.size;
    status = sixel_palette_resize_entries(palette,
                                          ncolors,
                                          depth,
                                          work_allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    payload_size = (size_t)ncolors * (size_t)depth;
    if (payload_size > 0U && palette->entries != NULL) {
        unsigned int i;
        unsigned int plane;

        for (i = 0U; i < ncolors; ++i) {
            for (plane = 0U; plane < depth; ++plane) {
                palette->entries[i * depth + plane]
                    = (unsigned char)colormap.table[i]->tuple[plane];
            }
        }
    }
    if (palette->use_reversible && palette->entries != NULL) {
        sixel_quant_reversible_palette(palette->entries,
                                       ncolors,
                                       depth);
    }
    status = SIXEL_OK;

success:
    palette->entry_count = ncolors;
    palette->original_colors = origcolors;
    palette->depth = (int)depth;

end:
    if (colormap.table != NULL) {
        sixel_allocator_free(work_allocator, colormap.table);
    }
    if (kmeans_entries != NULL) {
        sixel_allocator_free(work_allocator, kmeans_entries);
    }
    return status;
}

/*
 * Future changes will continue migrating quantization helpers into this file
 * so palette.c becomes the single coordination point for palette lifecycle
 * and generation.
 */
