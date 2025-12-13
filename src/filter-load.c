/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#include "config.h"

/* STDC_HEADERS */
#include <stdlib.h>
#if HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */

#include <sixel.h>

#include "filter-load.h"
#include "filter.h"
#include "status.h"

typedef struct sixel_filter_load_state {
    sixel_filter_load_config_t config;
} sixel_filter_load_state_t;

static SIXELSTATUS
sixel_filter_load_apply(sixel_filter_t *filter,
                        sixel_allocator_t *allocator,
                        sixel_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_load_state_t *state;
    sixel_frame_t **frame_slot;

    status = SIXEL_FALSE;
    state = NULL;
    frame_slot = NULL;

    if (filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_load_state_t *)filter->userdata;
    if (state == NULL || state->config.loader == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (filter->output.slot != NULL) {
        frame_slot = filter->output.slot;
    } else {
        frame_slot = state->config.frame_slot;
    }

    if (frame_slot == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = state->config.loader(state->config.loader_userdata,
                                  frame_slot,
                                  allocator,
                                  logger);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    filter->progress.total_units = 1;
    filter->progress.completed_units = 1;

    status = sixel_filter_update_progress(filter, 1);
    if (status == SIXEL_FALSE) {
        status = SIXEL_OK;
    }

    return status;
}

static void
sixel_filter_load_dispose(sixel_filter_t *filter)
{
    sixel_filter_load_state_t *state;

    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_load_state_t *)filter->userdata;
    if (state != NULL) {
        free(state);
        filter->userdata = NULL;
    }
}

SIXELSTATUS
sixel_filter_load_init(sixel_filter_t *filter,
                       const sixel_filter_load_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_load_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL || config == NULL || config->loader == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_load_state_t *)malloc(sizeof(*state));
    if (state == NULL) {
        sixel_helper_set_additional_message(
            "sixel_filter_load_init: malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(state, 0, sizeof(*state));
    state->config = *config;

    status = sixel_filter_init(filter,
                               "load",
                               SIXEL_FILTER_KIND_LOAD,
                               sixel_filter_load_apply,
                               sixel_filter_load_dispose,
                               state);
    if (SIXEL_FAILED(status)) {
        free(state);
        return status;
    }

    sixel_filter_set_progress(filter, NULL, NULL, 1);

    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
