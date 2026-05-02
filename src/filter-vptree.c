/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Filter wrapper for VP-tree lookup preparation.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "filter-lookup.h"
#include "filter-vptree.h"
#include "filter.h"
#include "status.h"

/*
 * Internal state retained by the VP-tree filter. The filter uses the lookup
 * builder to assemble a VP-tree-specific LUT and tracks ownership so dispose
 * can release it when the caller does not take over.
 */
typedef struct sixel_filter_vptree_state {
    sixel_filter_vptree_config_t config;
    sixel_filter_lookup_result_t result;
} sixel_filter_vptree_state_t;

static SIXELSTATUS
sixel_filter_vptree_apply(sixel_filter_t *filter,
      sixel_allocator_t *allocator,
      sixel_timeline_logger_t *logger);

static void
sixel_filter_vptree_dispose(sixel_filter_t *filter);

static sixel_filter_vtbl_t const sixel_filter_vptree_vtbl = {
    "vptree",
    SIXEL_FILTER_KIND_VPTREE,
    sixel_filter_vptree_apply,
    sixel_filter_vptree_dispose,
    NULL,
    NULL,
    NULL
};

static SIXELSTATUS
sixel_filter_vptree_apply(sixel_filter_t *filter,
                          sixel_allocator_t *allocator,
                          sixel_timeline_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_vptree_state_t *state;
    sixel_filter_lookup_result_t *result_out;

    status = SIXEL_FALSE;
    state = NULL;
    result_out = NULL;

    if (filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_vptree_state_t *)filter->userdata;
    if (state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (state->config.lookup_config.lut_policy != SIXEL_LUT_POLICY_VPTREE) {
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
        state->result.policy = NULL;
        state->result.owned = 0;
    }

    filter->progress.total_units = 1;
    filter->progress.completed_units = 1;
    (void)sixel_filter_update_progress(filter, 1);

    return SIXEL_OK;
}

static void
sixel_filter_vptree_dispose(sixel_filter_t *filter)
{
    sixel_filter_vptree_state_t *state;

    state = NULL;
    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_vptree_state_t *)filter->userdata;
    if (state != NULL) {
        if (state->result.owned != 0 && state->result.policy != NULL) {
            state->result.policy->vtbl->unref(state->result.policy);
        }
        free(state);
        filter->userdata = NULL;
    }
}

SIXELSTATUS
sixel_filter_vptree_init(sixel_filter_t *filter,
                         const sixel_filter_vptree_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_vptree_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (config->lookup_config.lut_policy != SIXEL_LUT_POLICY_VPTREE) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_vptree_state_t *)calloc(
        1u, sizeof(sixel_filter_vptree_state_t));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    state->config = *config;

    status = sixel_filter_init_with_vtbl(
        filter,
        &sixel_filter_vptree_vtbl,
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
