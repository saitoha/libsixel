/*
 * SPDX-License-Identifier: MIT
 *
 * Load filter tests. These verify that loader callbacks can write into either
 * the provided frame slot or an explicitly bound output slot while reporting
 * progress.
 */

#include "config.h"

#include <stdio.h>

#include <sixel.h>

#include "filter-factory.h"
#include "filter-load.h"
#include "filter.h"
#include "filter_test_common.h"

static SIXELSTATUS
stub_loader(void *userdata,
            sixel_frame_t **frame_out,
            sixel_allocator_t *allocator,
            sixel_logger_t *logger)
{
    SIXELSTATUS status;
    int *dimensions;
    int width;
    int height;

    (void)logger;

    status = SIXEL_FALSE;
    dimensions = NULL;
    width = 2;
    height = 2;

    if (userdata != NULL) {
        dimensions = (int *)userdata;
        width = dimensions[0];
        height = dimensions[1];
    }

    status = make_rgb_frame(allocator, width, height, frame_out);

    return status;
}

static int
test_load_filter_uses_config_slot(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_load_config_t config;
    sixel_frame_t *loaded;
    sixel_frame_t *bound_output;
    test_progress_t progress;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    loaded = NULL;
    bound_output = NULL;
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.loader = stub_loader;
    config.loader_userdata = NULL;
    config.frame_slot = &loaded;

    status = sixel_filter_factory_create_by_name("load", &config, &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (loaded == NULL || loaded->width != 2 || loaded->height != 2) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (bound_output != NULL) {
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
    sixel_frame_unref(loaded);
    sixel_frame_unref(bound_output);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_load_filter_prefers_bound_output_slot(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_load_config_t config;
    sixel_frame_t *config_slot;
    sixel_frame_t *bound_output;
    int dims[2];

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    config_slot = NULL;
    bound_output = NULL;
    dims[0] = 3;
    dims[1] = 4;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.loader = stub_loader;
    config.loader_userdata = dims;
    config.frame_slot = &config_slot;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_LOAD,
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_output(filter,
                             &bound_output,
                             SIXEL_PIXELFORMAT_RGB888,
                             SIXEL_COLORSPACE_GAMMA);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (bound_output == NULL || bound_output->width != 3
            || bound_output->height != 4) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (config_slot != NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

cleanup:
    sixel_filter_teardown(filter);
    sixel_filter_free(filter);
    sixel_frame_unref(config_slot);
    sixel_frame_unref(bound_output);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

int main(void)
{
    int success;

    success = 1;
    printf("1..2\n");

    if (test_load_filter_uses_config_slot()) {
        printf("ok 1 - load filter uses config slot when no binding is set\n");
    } else {
        printf("not ok 1 - load filter uses config slot when no binding is "
               "set\n");
        success = 0;
    }

    if (test_load_filter_prefers_bound_output_slot()) {
        printf("ok 2 - load filter writes into bound output slot when "
               "present\n");
    } else {
        printf("not ok 2 - load filter writes into bound output slot when "
               "present\n");
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
