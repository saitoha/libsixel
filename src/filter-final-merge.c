/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#include "config.h"

/* STDC_HEADERS */
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "dither.h"
#include "filter-final-merge.h"
#include "filter.h"
#include "status.h"

typedef struct sixel_filter_final_merge_state {
    sixel_filter_final_merge_config_t config;
} sixel_filter_final_merge_state_t;

static SIXELSTATUS
sixel_filter_final_merge_apply_filter(sixel_filter_t *filter,
                                      sixel_allocator_t *allocator,
                                      sixel_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_final_merge_state_t *state;

    (void)allocator;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_final_merge_state_t *)filter->userdata;
    if (state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_filter_final_merge_apply(&state->config, logger);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    (void)sixel_filter_update_progress(filter, 1);

    return SIXEL_OK;
}

static void
sixel_filter_final_merge_dispose(sixel_filter_t *filter)
{
    sixel_filter_final_merge_state_t *state;

    state = NULL;
    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_final_merge_state_t *)filter->userdata;
    if (state != NULL) {
        free(state);
        filter->userdata = NULL;
    }
}

SIXELSTATUS
sixel_filter_final_merge_init(sixel_filter_t *filter,
                              const sixel_filter_final_merge_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_final_merge_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_final_merge_state_t *)malloc(sizeof(*state));
    if (state == NULL) {
        sixel_helper_set_additional_message(
            "sixel_filter_final_merge_init: malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(state, 0, sizeof(*state));
    state->config = *config;

    status = sixel_filter_init(filter,
                               "final-merge",
                               SIXEL_FILTER_KIND_FINAL_MERGE,
                               sixel_filter_final_merge_apply_filter,
                               sixel_filter_final_merge_dispose,
                               state);
    if (SIXEL_FAILED(status)) {
        free(state);
        return status;
    }

    sixel_filter_set_progress(filter, NULL, NULL, 1);

    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_filter_final_merge_apply(const sixel_filter_final_merge_config_t *config,
                               sixel_logger_t *logger)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;

    if (config == NULL || config->dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_dither_set_final_merge(config->dither, config->final_merge_mode);

    if (logger != NULL) {
        sixel_logger_logf(logger,
                          "filter",
                          "worker",
                          "final-merge",
                          -1,
                          -1,
                          0,
                          0,
                          0,
                          0,
                          "mode=%d",
                          config->final_merge_mode);
    }

    status = SIXEL_OK;

    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
