/*
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the lookup filter. These tests validate LUT allocation,
 * reuse, and progress reporting through the common filter interface.
 */

#include "config.h"

#include <stdio.h>

#include <sixel.h>

#include "filter-factory.h"
#include "filter-lookup.h"
#include "filter.h"
#include "filter_test_common.h"

static int
test_lookup_build_allocates_owned_lut(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_lookup_config_t config;
    sixel_filter_lookup_result_t result;
    unsigned char palette[6];
    unsigned char pixel[3];
    int mapped;

    status = SIXEL_FALSE;
    allocator = NULL;
    result.lut = NULL;
    result.owned = 0;
    palette[0] = 0;
    palette[1] = 0;
    palette[2] = 0;
    palette[3] = 255;
    palette[4] = 0;
    palette[5] = 0;
    pixel[0] = 255;
    pixel[1] = 0;
    pixel[2] = 0;
    mapped = -1;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.palette = palette;
    config.depth = 3;
    config.ncolors = 2;
    config.complexion = 1;
    config.method_for_largest = SIXEL_LARGE_AUTO;
    config.lut_policy = SIXEL_LUT_POLICY_AUTO;
    config.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    config.reuse_lut = NULL;

    status = sixel_filter_lookup_build(&config,
                                       allocator,
                                       NULL,
                                       &result);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (result.lut == NULL || result.owned == 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    mapped = sixel_lut_map_pixel(result.lut, pixel);
    if (mapped < 0 || mapped >= config.ncolors) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

cleanup:
    if (result.lut != NULL && result.owned != 0) {
        sixel_lut_unref(result.lut);
    }
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_lookup_filter_reuses_lut_and_reports_progress(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_lookup_config_t config;
    sixel_lut_t *reuse_lut;
    unsigned char palette[6];
    unsigned char pixel[3];
    test_progress_t progress;
    int mapped;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    reuse_lut = NULL;
    palette[0] = 0;
    palette[1] = 0;
    palette[2] = 255;
    palette[3] = 0;
    palette[4] = 255;
    palette[5] = 0;
    pixel[0] = 0;
    pixel[1] = 255;
    pixel[2] = 0;
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;
    mapped = -1;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_lut_new(&reuse_lut, SIXEL_LUT_POLICY_AUTO, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.palette = palette;
    config.depth = 3;
    config.ncolors = 2;
    config.complexion = 1;
    config.method_for_largest = SIXEL_LARGE_AUTO;
    config.lut_policy = SIXEL_LUT_POLICY_AUTO;
    config.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    config.reuse_lut = reuse_lut;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_LOOKUP,
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    mapped = sixel_lut_map_pixel(reuse_lut, pixel);
    if (mapped < 0 || mapped >= config.ncolors) {
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
    if (reuse_lut != NULL) {
        sixel_lut_unref(reuse_lut);
    }
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

int main(void)
{
    int success;

    success = 1;
    printf("1..2\n");

    if (test_lookup_build_allocates_owned_lut()) {
        printf("ok 1 - lookup filter allocates owned lut\n");
    } else {
        printf("not ok 1 - lookup filter allocates owned lut\n");
        success = 0;
    }

    if (test_lookup_filter_reuses_lut_and_reports_progress()) {
        printf("ok 2 - lookup filter reuses lut and reports progress\n");
    } else {
        printf("not ok 2 - lookup filter reuses lut and reports progress\n");
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
