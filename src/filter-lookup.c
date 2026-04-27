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
#include "status.h"

/*
 * IDL usage in this unit
 *
 * IComponents.getservice("services/factory", &factory)
 * IFactory.create("lookup/...", allocator, &policy)
 * ILookupPolicy.prepare(request{shared_instance_enabled,...})
 */

/*
 * Internal state retained by the lookup filter. The filter creates a prepared
 * lookup policy object and tracks ownership so dispose can release it.
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

static int
sixel_filter_lookup_default_shared_instance_enabled(int lut_policy)
{
    if (lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        return sixel_lookup_policy_certlut_shared_instance_enabled();
    }
    if (lut_policy == SIXEL_LUT_POLICY_5BIT) {
        return sixel_lookup_policy_5bit_shared_instance_enabled();
    }
    if (lut_policy == SIXEL_LUT_POLICY_6BIT) {
        return sixel_lookup_policy_6bit_shared_instance_enabled();
    }

    return 1;
}

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
    sixel_lookup_policy_prepare_request_t prepare_request;
    sixel_lookup_policy_interface_t *policy;
    sixel_factory_t *factory;
    void *service;
    char const *policy_name;
    int optimize_lookup;
    float const *palette_float;
    int float_depth;

    status = SIXEL_FALSE;
    memset(&result, 0, sizeof(result));
    memset(&select_request, 0, sizeof(select_request));
    memset(&prepare_request, 0, sizeof(prepare_request));
    policy = NULL;
    factory = NULL;
    service = NULL;
    policy_name = NULL;
    optimize_lookup = 0;
    palette_float = NULL;
    float_depth = 0;

    if (config == NULL || result_out == NULL
            || config->palette == NULL
            || config->depth <= 0
            || config->ncolors <= 0
            || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    palette_float = config->palette_float;
    /* Reset float_depth when palette_float is absent to avoid MSan noise. */
    if (palette_float == NULL) {
        float_depth = 0;
    } else {
        float_depth = config->float_depth;
    }
    optimize_lookup = (config->lut_policy != SIXEL_LUT_POLICY_NONE);
    select_request.palette = config->palette;
    select_request.depth = config->depth;
    select_request.reqcolor = config->ncolors;
    select_request.optimize_lookup = optimize_lookup;
    select_request.lut_policy = config->lut_policy;
    select_request.pixelformat = config->pixelformat;
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
                                   allocator,
                                   (void **)&policy);
    factory->vtbl->unref(factory);
    factory = NULL;
    if (SIXEL_FAILED(status)) {
        return status;
    }

    prepare_request.palette = config->palette;
    prepare_request.palette_float = palette_float;
    prepare_request.depth = config->depth;
    prepare_request.float_depth = float_depth;
    prepare_request.reqcolor = config->ncolors;
    prepare_request.pixelformat = config->pixelformat;
    prepare_request.parallel_dither_active = 0;
    prepare_request.shared_instance_enabled =
        sixel_filter_lookup_default_shared_instance_enabled(
            config->lut_policy);
    prepare_request.reuse_policy = config->reuse_policy;
    prepare_request.reuse_policy_slot = NULL;
    prepare_request.allocator = allocator;
    status = policy->vtbl->prepare(policy, &prepare_request);
    if (SIXEL_FAILED(status)) {
        policy->vtbl->unref(policy);
        return status;
    }

    result.policy = policy;
    result.owned = 1;

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
