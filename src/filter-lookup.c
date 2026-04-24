/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "components.h"
#include "factory.h"
#include "filter-lookup.h"
#include "filter.h"
#include "lookup-policy.h"
#include "status.h"

/*
 * IDL (internal contract)
 *
 * class LookupFilterState {
 *   config: ILookupFilterConfig;
 *   result: ILookupFilterResult;
 * }
 *
 * Ownership/lifetime:
 * - State owns result.policy only when result.owned != 0.
 * - Dispose unrefs owned policy objects.
 */
typedef struct sixel_filter_lookup_state {
    sixel_filter_lookup_config_t config;
    sixel_filter_lookup_result_t result;
} sixel_filter_lookup_state_t;

static SIXELSTATUS
sixel_filter_lookup_apply(sixel_filter_t *filter,
      sixel_allocator_t *allocator,
      sixel_logger_t *logger);

static void
sixel_filter_lookup_dispose(sixel_filter_t *filter);

static sixel_filter_vtbl_t const sixel_filter_lookup_vtbl = {
    "lookup",
    SIXEL_FILTER_KIND_LOOKUP,
    sixel_filter_lookup_apply,
    sixel_filter_lookup_dispose,
    NULL,
    NULL,
    NULL
};

SIXELAPI SIXELSTATUS
sixel_filter_lookup_build(const sixel_filter_lookup_config_t *config,
                          sixel_allocator_t *allocator,
                          sixel_logger_t *logger,
                          sixel_filter_lookup_result_t *result_out)
{
    SIXELSTATUS status;
    sixel_filter_lookup_result_t result;
    sixel_lookup_policy_select_request_t select_request;
    sixel_lookup_policy_prepare_request_t request;
    sixel_factory_t *factory;
    void *service;
    char const *policy_name;
    int optimize_lookup;

    status = SIXEL_FALSE;
    memset(&result, 0, sizeof(result));
    memset(&select_request, 0, sizeof(select_request));
    memset(&request, 0, sizeof(request));
    factory = NULL;
    service = NULL;
    policy_name = NULL;
    optimize_lookup = 0;

    if (config == NULL || result_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    result.policy = config->reuse_policy;
    result.owned = 0;
    optimize_lookup = (config->lut_policy != SIXEL_LUT_POLICY_NONE);

    if (result.policy == NULL) {
        select_request.palette = config->palette;
        select_request.depth = config->depth;
        select_request.reqcolor = config->ncolors;
        select_request.optimize_lookup = optimize_lookup;
        select_request.lut_policy = config->lut_policy;
        policy_name = sixel_lookup_policy_select_name(&select_request);
        if (policy_name == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }

        status = sixel_components_getservice("services/factory", &service);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        factory = (sixel_factory_t *)service;
        status = factory->vtbl->create(factory,
                                       policy_name,
                                       (void **)&result.policy);
        factory->vtbl->unref(factory);
        factory = NULL;
        if (SIXEL_FAILED(status)) {
            return status;
        }
        result.owned = 1;
    }

    request.palette = config->palette;
    request.palette_float = config->palette_float;
    request.depth = config->depth;
    request.float_depth = config->float_depth;
    request.reqcolor = config->ncolors;
    request.complexion = config->complexion;
    request.pixelformat = config->pixelformat;
    request.reuse_policy = config->reuse_policy;
    request.reuse_policy_slot = config->reuse_policy_slot;
    request.allocator = allocator;

    status = result.policy->vtbl->prepare(result.policy, &request);
    if (SIXEL_FAILED(status)) {
        if (result.owned != 0 && result.policy != NULL) {
            result.policy->vtbl->unref(result.policy);
        }
        return status;
    }

    if (result.owned != 0
            && config->reuse_policy_slot != NULL
            && *config->reuse_policy_slot == NULL) {
        *config->reuse_policy_slot = result.policy;
        result.owned = 0;
    }

    if (logger != NULL) {
        sixel_logger_logf(logger,
                          "filter",
                          "worker",
                          "lookup-configured",
                          -1,
                          -1,
                          0,
                          0,
                          0,
                          0,
                          "policy=%d depth=%d colors=%d owned=%d",
                          config->lut_policy,
                          config->depth,
                          config->ncolors,
                          result.owned);
    }

    *result_out = result;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_filter_lookup_apply(sixel_filter_t *filter,
                          sixel_allocator_t *allocator,
                          sixel_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_lookup_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_lookup_state_t *)filter->userdata;
    if (state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_filter_lookup_build(&state->config,
                                       allocator,
                                       logger,
                                       &state->result);
    if (SIXEL_SUCCEEDED(status)) {
        filter->progress.total_units = 1;
        filter->progress.completed_units = 1;
        sixel_filter_update_progress(filter, 1);
    }

    return status;
}

static void
sixel_filter_lookup_dispose(sixel_filter_t *filter)
{
    sixel_filter_lookup_state_t *state;

    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_lookup_state_t *)filter->userdata;
    if (state != NULL) {
        if (state->result.owned != 0 && state->result.policy != NULL) {
            state->result.policy->vtbl->unref(state->result.policy);
        }
        free(state);
    }
}

SIXELSTATUS
sixel_filter_lookup_init(sixel_filter_t *filter,
                         const sixel_filter_lookup_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_lookup_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_lookup_state_t *)calloc(
        1u, sizeof(sixel_filter_lookup_state_t));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    state->config = *config;

    status = sixel_filter_init_with_vtbl(
        filter,
        &sixel_filter_lookup_vtbl,
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
