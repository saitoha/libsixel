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

#include "filter-lookup.h"
#include "filter-vpte.h"
#include "filter.h"
#include "status.h"

/*
 * Internal state retained by the VPTE filter. The filter uses the lookup
 * builder to assemble a VPTE-specific LUT and tracks ownership so dispose can
 * release it when the caller does not take over.
 */
typedef struct sixel_filter_vpte_state {
    sixel_filter_vpte_config_t config;
    sixel_filter_lookup_result_t result;
} sixel_filter_vpte_state_t;

static SIXELSTATUS
sixel_filter_vpte_apply(sixel_filter_t *filter,
                        sixel_allocator_t *allocator,
                        sixel_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_vpte_state_t *state;
    sixel_filter_lookup_result_t *result_out;

    status = SIXEL_FALSE;
    state = NULL;
    result_out = NULL;

    if (filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_vpte_state_t *)filter->userdata;
    if (state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (state->config.lookup_config.lut_policy != SIXEL_LUT_POLICY_VPTE) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_filter_lookup_build(&state->config.lookup_config,
                                       allocator,
                                       logger,
                                       &state->result);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    result_out = state->config.result_out;
    if (result_out != NULL) {
        *result_out = state->result;
        state->result.lut = NULL;
        state->result.owned = 0;
    }

    filter->progress.total_units = 1;
    filter->progress.completed_units = 1;
    (void)sixel_filter_update_progress(filter, 1);

    return SIXEL_OK;
}

static void
sixel_filter_vpte_dispose(sixel_filter_t *filter)
{
    sixel_filter_vpte_state_t *state;

    state = NULL;
    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_vpte_state_t *)filter->userdata;
    if (state != NULL) {
        if (state->result.owned != 0 && state->result.lut != NULL) {
            sixel_lut_unref(state->result.lut);
        }
        free(state);
        filter->userdata = NULL;
    }
}

SIXELSTATUS
sixel_filter_vpte_init(sixel_filter_t *filter,
                       const sixel_filter_vpte_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_vpte_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (config->lookup_config.lut_policy != SIXEL_LUT_POLICY_VPTE) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_vpte_state_t *)calloc(
        1u, sizeof(sixel_filter_vpte_state_t));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    state->config = *config;

    status = sixel_filter_init(filter,
                               "vpte",
                               SIXEL_FILTER_KIND_VPTE,
                               sixel_filter_vpte_apply,
                               sixel_filter_vpte_dispose,
                               state);
    if (SIXEL_FAILED(status)) {
        free(state);
        return status;
    }

    filter->progress.total_units = 1;
    filter->progress.completed_units = 0;

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
