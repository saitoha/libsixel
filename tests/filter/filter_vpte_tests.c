/*
 * SPDX-License-Identifier: MIT
 *
 * VPTE filter tests. These confirm VPTE-only LUT builds, ownership handoff,
 * and progress reporting through the filter facade.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <sixel.h>

#include "filter-factory.h"
#include "filter-vpte.h"
#include "filter.h"
#include "filter_test_common.h"

static int
test_vpte_builds_owned_lut_and_transfers_result(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_vpte_config_t config;
    sixel_filter_lookup_result_t result;
    unsigned char palette[6];
    unsigned char pixel[3];
    test_progress_t progress;
    int mapped;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
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
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;
    mapped = -1;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.lookup_config.palette = palette;
    config.lookup_config.depth = 3;
    config.lookup_config.ncolors = 2;
    config.lookup_config.complexion = 1;
    config.lookup_config.lut_policy = SIXEL_LUT_POLICY_VPTE;
    config.lookup_config.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    config.lookup_config.reuse_lut = NULL;
    config.lookup_config.method_for_largest = SIXEL_LARGE_AUTO;
    config.result_out = &result;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_VPTE,
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

    if (result.lut == NULL || result.owned == 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    mapped = sixel_lut_map_pixel(result.lut, pixel);
    if (mapped < 0 || mapped >= config.lookup_config.ncolors) {
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
    if (result.lut != NULL && result.owned != 0) {
        sixel_lut_unref(result.lut);
    }
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_vpte_init_rejects_non_vpte_policy(void)
{
    SIXELSTATUS status;
    sixel_filter_t *filter;
    sixel_filter_vpte_config_t config;

    status = SIXEL_FALSE;
    filter = NULL;

    config.lookup_config.palette = NULL;
    config.lookup_config.depth = 0;
    config.lookup_config.ncolors = 0;
    config.lookup_config.complexion = 1;
    config.lookup_config.method_for_largest = SIXEL_LARGE_AUTO;
    config.lookup_config.lut_policy = SIXEL_LUT_POLICY_AUTO;
    config.lookup_config.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    config.lookup_config.reuse_lut = NULL;
    config.result_out = NULL;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_VPTE,
                                                 &config,
                                                 &filter);
    if (status != SIXEL_BAD_ARGUMENT) {
        sixel_filter_teardown(filter);
        sixel_filter_free(filter);
        return 0;
    }

    return 1;
}

int main(void)
{
    int success;

    success = 1;
    printf("1..2\n");

    if (test_vpte_builds_owned_lut_and_transfers_result()) {
        printf("ok 1 - vpte filter builds lut and reports progress\n");
    } else {
        printf("not ok 1 - vpte filter builds lut and reports progress\n");
        success = 0;
    }

    if (test_vpte_init_rejects_non_vpte_policy()) {
        printf("ok 2 - vpte init rejects non-vpte policy\n");
    } else {
        printf("not ok 2 - vpte init rejects non-vpte policy\n");
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
