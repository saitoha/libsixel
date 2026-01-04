/*
 * SPDX-License-Identifier: MIT
 *
 * Final-merge filter tests. These verify that the filter updates the dither
 * policy and reports progress through the shared filter interface.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>

#include <sixel.h>

#include "dither.h"
#include "filter-factory.h"
#include "filter-final-merge.h"
#include "filter.h"
#include "filter_test_common.h"

static int
test_final_merge_sets_mode_and_progress(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_final_merge_config_t config;
    sixel_dither_t *dither;
    test_progress_t progress;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    dither = NULL;
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_dither(allocator, 8, &dither);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.dither = dither;
    config.final_merge_mode = SIXEL_FINAL_MERGE_WARD;

    status = sixel_filter_factory_create_by_kind(
        SIXEL_FILTER_KIND_FINAL_MERGE,
        &config,
        &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    if (dither->final_merge_mode != SIXEL_FINAL_MERGE_AUTO) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (dither->final_merge_mode != SIXEL_FINAL_MERGE_WARD) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (progress.began != 1 || progress.completed != 1 || progress.aborted) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

cleanup:
    sixel_filter_teardown(filter);
    sixel_filter_free(filter);
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_final_merge_direct_apply_updates_palette(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_palette_t *palette;
    sixel_filter_final_merge_config_t config;

    status = SIXEL_FALSE;
    allocator = NULL;
    dither = NULL;
    palette = NULL;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_dither(allocator, 16, &dither);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_palette_new(&palette, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_palette_unref(dither->palette);
    dither->palette = palette;

    config.dither = dither;
    config.final_merge_mode = SIXEL_FINAL_MERGE_NONE;

    status = sixel_filter_final_merge_apply(&config, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (dither->final_merge_mode != SIXEL_FINAL_MERGE_NONE) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (dither->palette->final_merge != SIXEL_FINAL_MERGE_NONE) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

cleanup:
    if (dither != NULL) {
        sixel_dither_unref(dither);
    } else if (palette != NULL) {
        sixel_palette_unref(palette);
    }
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

int main(void)
{
    int success;

    success = 1;
    printf("1..2\n");

    if (test_final_merge_sets_mode_and_progress()) {
        printf("ok 1 - final-merge filter sets mode and reports "
               "progress\n");
    } else {
        printf("not ok 1 - final-merge filter sets mode and reports "
               "progress\n");
        success = 0;
    }

    if (test_final_merge_direct_apply_updates_palette()) {
        printf("ok 2 - final-merge apply updates palette and dither\n");
    } else {
        printf("not ok 2 - final-merge apply updates palette and dither\n");
        success = 0;
    }

    return success ? 0 : 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
