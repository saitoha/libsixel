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

#include "filter-lookup.h"
#include "filter.h"
#include "lookup-common.h"
#include "status.h"

/*
 * Internal state retained by the lookup filter. The filter either reuses a
 * caller-provided LUT or allocates a new one and marks ownership so dispose can
 * release it.
 */
typedef struct sixel_filter_lookup_state {
    sixel_filter_lookup_config_t config;
    sixel_filter_lookup_result_t result;
} sixel_filter_lookup_state_t;

static void
sixel_filter_lookup_weights(const sixel_filter_lookup_config_t *config,
                            int *wcomp1_out,
                            int *wcomp2_out,
                            int *wcomp3_out)
{
    int wcomp1;
    int wcomp2;
    int wcomp3;

    wcomp1 = 1;
    wcomp2 = 1;
    wcomp3 = 1;

    if (config != NULL
        && config->lut_policy == SIXEL_LUT_POLICY_CERTLUT
        && config->method_for_largest == SIXEL_LARGE_LUM) {
        wcomp1 = config->complexion * 299;
        wcomp2 = 587;
        wcomp3 = 114;
    } else if (config != NULL) {
        wcomp1 = config->complexion;
    }

    *wcomp1_out = wcomp1;
    *wcomp2_out = wcomp2;
    *wcomp3_out = wcomp3;
}

SIXELAPI SIXELSTATUS
sixel_filter_lookup_build(const sixel_filter_lookup_config_t *config,
                          sixel_allocator_t *allocator,
                          sixel_logger_t *logger,
                          sixel_filter_lookup_result_t *result_out)
{
    SIXELSTATUS status;
    sixel_filter_lookup_result_t result;
    int wcomp1;
    int wcomp2;
    int wcomp3;
    float const *palette_float;
    int float_depth;

    status = SIXEL_FALSE;
    memset(&result, 0, sizeof(result));
    wcomp1 = 0;
    wcomp2 = 0;
    wcomp3 = 0;
    palette_float = NULL;
    float_depth = 0;

    if (config == NULL || result_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_filter_lookup_weights(config, &wcomp1, &wcomp2, &wcomp3);

    result.lut = config->reuse_lut;
    result.owned = 0;

    if (result.lut == NULL) {
        status = sixel_lut_new(&result.lut, config->lut_policy, allocator);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_filter_lookup_build: failed to allocate LUT.");
            return status;
        }
        result.owned = 1;
    }

    palette_float = config->palette_float;
    float_depth = config->float_depth;
    status = sixel_lut_configure(result.lut,
                                 config->palette,
                                 palette_float,
                                 config->depth,
                                 float_depth,
                                 config->ncolors,
                                 config->complexion,
                                 wcomp1,
                                 wcomp2,
                                 wcomp3,
                                 config->lut_policy,
                                 config->pixelformat);
    if (SIXEL_FAILED(status)) {
        if (result.owned != 0 && result.lut != NULL) {
            sixel_lut_unref(result.lut);
        }
        return status;
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
        if (state->result.owned != 0 && state->result.lut != NULL) {
            sixel_lut_unref(state->result.lut);
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

    status = sixel_filter_init(filter,
                               "lookup",
                               SIXEL_FILTER_KIND_LOOKUP,
                               sixel_filter_lookup_apply,
                               sixel_filter_lookup_dispose,
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
