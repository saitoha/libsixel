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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdint.h>

#include <sixel.h>

#include "palette-plan.h"

static int
sixel_palette_policy_origin_is_valid(sixel_palette_policy_origin_t origin)
{
    return origin >= SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT
        && origin <= SIXEL_PALETTE_POLICY_ORIGIN_LEGACY_ALIAS;
}

static int
sixel_palette_resolution_reason_is_valid(
    sixel_palette_resolution_reason_t reason)
{
    return reason > SIXEL_PALETTE_RESOLUTION_NONE
        && reason <= SIXEL_PALETTE_RESOLUTION_NOT_APPLICABLE;
}

void
sixel_palette_policy_resolution_init(
    sixel_palette_policy_resolution_t *resolution,
    int requested,
    sixel_palette_policy_origin_t origin)
{
    if (resolution == NULL) {
        return;
    }

    resolution->requested = requested;
    resolution->effective = SIXEL_PALETTE_POLICY_VALUE_UNSET;
    resolution->origin = sixel_palette_policy_origin_is_valid(origin)
        ? origin : SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT;
    resolution->phase = SIXEL_PALETTE_POLICY_UNRESOLVED;
    resolution->reason = SIXEL_PALETTE_RESOLUTION_NONE;
}

SIXELSTATUS
sixel_palette_policy_resolve(
    sixel_palette_policy_resolution_t *resolution,
    int effective,
    sixel_palette_resolution_reason_t reason)
{
    if (resolution == NULL ||
            !sixel_palette_resolution_reason_is_valid(reason) ||
            reason == SIXEL_PALETTE_RESOLUTION_NOT_APPLICABLE) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (resolution->phase != SIXEL_PALETTE_POLICY_UNRESOLVED) {
        return SIXEL_LOGIC_ERROR;
    }

    resolution->effective = effective;
    resolution->phase = SIXEL_PALETTE_POLICY_RESOLVED;
    resolution->reason = reason;

    return SIXEL_OK;
}

SIXELSTATUS
sixel_palette_policy_mark_executed(
    sixel_palette_policy_resolution_t *resolution)
{
    if (resolution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (resolution->phase != SIXEL_PALETTE_POLICY_RESOLVED) {
        return SIXEL_LOGIC_ERROR;
    }

    resolution->phase = SIXEL_PALETTE_POLICY_EXECUTED;

    return SIXEL_OK;
}

SIXELSTATUS
sixel_palette_sampling_resolve(
    sixel_palette_frame_state_t *state,
    sixel_palette_sampling_policy_t effective,
    sixel_palette_sampling_source_t source,
    sixel_palette_resolution_reason_t reason)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;
    if (state == NULL ||
            (effective != SIXEL_PALETTE_SAMPLING_FULL_FRAME &&
             effective != SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID) ||
            (source != SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME &&
             source !=
                SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME)) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_palette_policy_resolve(&state->sampling,
                                          effective,
                                          reason);
    if (SIXEL_SUCCEEDED(status)) {
        state->sampling_source = source;
    }
    return status;
}

sixel_palette_sampling_policy_t
sixel_palette_sampling_select_auto(int total_threads,
                                   int heavy_operations,
                                   int async_eligible)
{
    int available;

    available = total_threads - heavy_operations;
    if (async_eligible != 0 && total_threads > 1 && available > 1) {
        return SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID;
    }
    return SIXEL_PALETTE_SAMPLING_FULL_FRAME;
}

SIXELSTATUS
sixel_palette_sampling_resolve_auto(sixel_palette_frame_state_t *state,
                                    int total_threads,
                                    int heavy_operations,
                                    int async_eligible)
{
    sixel_palette_sampling_policy_t effective;
    sixel_palette_sampling_source_t source;

    if (state == NULL || total_threads < 1 || heavy_operations < 0 ||
            state->sampling.requested != SIXEL_PALETTE_SAMPLING_AUTO) {
        return SIXEL_BAD_ARGUMENT;
    }

    effective = sixel_palette_sampling_select_auto(total_threads,
                                                    heavy_operations,
                                                    async_eligible);
    source = effective == SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID
        ? SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME
        : SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME;
    return sixel_palette_sampling_resolve(
        state,
        effective,
        source,
        SIXEL_PALETTE_RESOLUTION_RESOURCE_PROFILE);
}

SIXELSTATUS
sixel_palette_sampling_resolve_fallback(
    sixel_palette_frame_state_t *state)
{
    sixel_palette_policy_resolution_t *sampling;

    if (state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sampling = &state->sampling;
    /*
     * This transition represents the successful path that will construct the
     * final palette, not the failed adaptive attempt.  Restrict it to an
     * automatic request so a future explicit sampling option cannot silently
     * change policy after execution has started.
     */
    if (sampling->requested != SIXEL_PALETTE_SAMPLING_AUTO ||
            sampling->origin != SIXEL_PALETTE_POLICY_ORIGIN_AUTO ||
            sampling->effective != SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID ||
            (sampling->phase != SIXEL_PALETTE_POLICY_RESOLVED &&
             sampling->phase != SIXEL_PALETTE_POLICY_EXECUTED) ||
            state->sampling_source !=
                SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME) {
        return SIXEL_LOGIC_ERROR;
    }

    sampling->effective = SIXEL_PALETTE_SAMPLING_FULL_FRAME;
    sampling->phase = SIXEL_PALETTE_POLICY_RESOLVED;
    sampling->reason = SIXEL_PALETTE_RESOLUTION_FALLBACK;
    state->sampling_source =
        SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME;

    return SIXEL_OK;
}

static int
sixel_palette_binning_policy_is_valid(
    sixel_palette_binning_policy_t policy)
{
    return policy >= SIXEL_PALETTE_BINNING_AUTO &&
        policy <= SIXEL_PALETTE_BINNING_SOFT;
}

static int
sixel_palette_binning_grid_map_is_valid(
    sixel_palette_binning_grid_map_t grid_map)
{
    return grid_map >= SIXEL_PALETTE_BINNING_GRID_NONE &&
        grid_map <= SIXEL_PALETTE_BINNING_GRID_SRGB;
}

static int
sixel_palette_binning_kernel_is_valid(
    sixel_palette_binning_kernel_t kernel)
{
    return kernel >= SIXEL_PALETTE_BINNING_KERNEL_NONE &&
        kernel <= SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR;
}

static int
sixel_palette_binning_backend_is_valid(
    sixel_palette_binning_backend_t backend)
{
    return backend >= SIXEL_PALETTE_BINNING_BACKEND_DIRECT &&
        backend <= SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE;
}

static void
sixel_palette_binning_metadata_reset(
    sixel_palette_binning_state_t *state)
{
    state->bits_per_axis = 0u;
    state->grid_map = SIXEL_PALETTE_BINNING_GRID_NONE;
    state->kernel = SIXEL_PALETTE_BINNING_KERNEL_NONE;
    state->backend = SIXEL_PALETTE_BINNING_BACKEND_UNRESOLVED;
    state->source_point_count = 0u;
    state->entry_capacity_bound = 0u;
}

void
sixel_palette_binning_state_init(
    sixel_palette_binning_state_t *state,
    sixel_palette_binning_policy_t requested,
    sixel_palette_policy_origin_t origin)
{
    if (state == NULL) {
        return;
    }

    sixel_palette_policy_resolution_init(&state->policy,
                                         requested,
                                         origin);
    sixel_palette_binning_metadata_reset(state);
}

static int
sixel_palette_binning_contract_is_valid(
    sixel_palette_binning_policy_t policy,
    unsigned int bits_per_axis,
    sixel_palette_binning_grid_map_t grid_map,
    sixel_palette_binning_kernel_t kernel,
    sixel_palette_binning_backend_t backend)
{
    if (!sixel_palette_binning_policy_is_valid(policy) ||
            policy == SIXEL_PALETTE_BINNING_AUTO ||
            !sixel_palette_binning_grid_map_is_valid(grid_map) ||
            !sixel_palette_binning_kernel_is_valid(kernel) ||
            !sixel_palette_binning_backend_is_valid(backend)) {
        return 0;
    }
    if (policy == SIXEL_PALETTE_BINNING_NONE) {
        return bits_per_axis == 0u &&
            grid_map == SIXEL_PALETTE_BINNING_GRID_NONE &&
            kernel == SIXEL_PALETTE_BINNING_KERNEL_NONE &&
            backend == SIXEL_PALETTE_BINNING_BACKEND_DIRECT;
    }
    if (policy == SIXEL_PALETTE_BINNING_EXACT) {
        return bits_per_axis == 0u &&
            grid_map == SIXEL_PALETTE_BINNING_GRID_NONE &&
            kernel == SIXEL_PALETTE_BINNING_KERNEL_NONE &&
            backend == SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE;
    }
    if (bits_per_axis < SIXEL_PALETTE_BINNING_MIN_BITS ||
            bits_per_axis > SIXEL_PALETTE_BINNING_MAX_BITS ||
            grid_map == SIXEL_PALETTE_BINNING_GRID_NONE ||
            (backend != SIXEL_PALETTE_BINNING_BACKEND_DENSE &&
             backend != SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE)) {
        return 0;
    }
    if (policy == SIXEL_PALETTE_BINNING_HARD) {
        return kernel == SIXEL_PALETTE_BINNING_KERNEL_NONE;
    }
    return kernel == SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR;
}

SIXELSTATUS
sixel_palette_binning_entry_bound(
    sixel_palette_binning_policy_t policy,
    unsigned int bits_per_axis,
    sixel_palette_binning_kernel_t kernel,
    size_t source_point_count,
    size_t *entry_capacity_bound)
{
    size_t bin_count;
    size_t domain_size;
    size_t contribution_count;
    unsigned int axis;

    bin_count = 0u;
    domain_size = 1u;
    contribution_count = source_point_count;
    axis = 0u;
    if (entry_capacity_bound == NULL || source_point_count == 0u ||
            !sixel_palette_binning_policy_is_valid(policy) ||
            policy == SIXEL_PALETTE_BINNING_AUTO ||
            !sixel_palette_binning_kernel_is_valid(kernel)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (policy == SIXEL_PALETTE_BINNING_NONE ||
            policy == SIXEL_PALETTE_BINNING_EXACT) {
        if (bits_per_axis != 0u ||
                kernel != SIXEL_PALETTE_BINNING_KERNEL_NONE) {
            return SIXEL_BAD_ARGUMENT;
        }
        *entry_capacity_bound = source_point_count;
        return SIXEL_OK;
    }
    if (bits_per_axis < SIXEL_PALETTE_BINNING_MIN_BITS ||
            bits_per_axis > SIXEL_PALETTE_BINNING_MAX_BITS) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((policy == SIXEL_PALETTE_BINNING_HARD &&
         kernel != SIXEL_PALETTE_BINNING_KERNEL_NONE) ||
            (policy == SIXEL_PALETTE_BINNING_SOFT &&
             kernel != SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR)) {
        return SIXEL_BAD_ARGUMENT;
    }

    bin_count = (size_t)1u << bits_per_axis;
    for (axis = 0u; axis < 3u; ++axis) {
        if (domain_size > SIZE_MAX / bin_count) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        domain_size *= bin_count;
    }
    if (policy == SIXEL_PALETTE_BINNING_SOFT) {
        if (source_point_count > SIZE_MAX / 8u) {
            contribution_count = SIZE_MAX;
        } else {
            contribution_count = source_point_count * 8u;
        }
    }
    *entry_capacity_bound = contribution_count < domain_size
        ? contribution_count : domain_size;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_palette_binning_resolve(
    sixel_palette_binning_state_t *state,
    sixel_palette_binning_policy_t effective,
    unsigned int bits_per_axis,
    sixel_palette_binning_grid_map_t grid_map,
    sixel_palette_binning_kernel_t kernel,
    sixel_palette_binning_backend_t backend,
    size_t source_point_count,
    sixel_palette_resolution_reason_t reason)
{
    SIXELSTATUS status;
    size_t entry_capacity_bound;

    status = SIXEL_FALSE;
    entry_capacity_bound = 0u;
    if (state == NULL ||
            !sixel_palette_binning_contract_is_valid(effective,
                                                      bits_per_axis,
                                                      grid_map,
                                                      kernel,
                                                      backend)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_palette_binning_policy_is_valid(
            (sixel_palette_binning_policy_t)state->policy.requested)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (state->policy.requested != SIXEL_PALETTE_BINNING_AUTO &&
            state->policy.requested != (int)effective) {
        return SIXEL_LOGIC_ERROR;
    }
    status = sixel_palette_binning_entry_bound(effective,
                                               bits_per_axis,
                                               kernel,
                                               source_point_count,
                                               &entry_capacity_bound);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_palette_policy_resolve(&state->policy,
                                          effective,
                                          reason);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    state->bits_per_axis = bits_per_axis;
    state->grid_map = grid_map;
    state->kernel = kernel;
    state->backend = backend;
    state->source_point_count = source_point_count;
    state->entry_capacity_bound = entry_capacity_bound;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_palette_binning_mark_bypassed(
    sixel_palette_binning_state_t *state)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;
    if (state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_palette_policy_mark_bypassed(&state->policy);
    if (SIXEL_SUCCEEDED(status)) {
        sixel_palette_binning_metadata_reset(state);
    }
    return status;
}

SIXELSTATUS
sixel_palette_policy_mark_bypassed(
    sixel_palette_policy_resolution_t *resolution)
{
    if (resolution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (resolution->phase != SIXEL_PALETTE_POLICY_UNRESOLVED) {
        return SIXEL_LOGIC_ERROR;
    }

    resolution->effective = SIXEL_PALETTE_POLICY_VALUE_UNSET;
    resolution->phase = SIXEL_PALETTE_POLICY_BYPASSED;
    resolution->reason = SIXEL_PALETTE_RESOLUTION_NOT_APPLICABLE;

    return SIXEL_OK;
}

void
sixel_palette_frame_state_init(sixel_palette_frame_state_t *state)
{
    if (state == NULL) {
        return;
    }

    sixel_palette_policy_resolution_init(
        &state->sampling,
        SIXEL_PALETTE_POLICY_VALUE_UNSET,
        SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT);
    state->sampling_source = SIXEL_PALETTE_SAMPLING_SOURCE_NONE;
    sixel_palette_policy_resolution_init(
        &state->binning.policy,
        SIXEL_PALETTE_POLICY_VALUE_UNSET,
        SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT);
    sixel_palette_binning_metadata_reset(&state->binning);
    sixel_palette_policy_resolution_init(
        &state->quantizer,
        SIXEL_PALETTE_POLICY_VALUE_UNSET,
        SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
