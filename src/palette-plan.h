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

#ifndef LIBSIXEL_PALETTE_PLAN_H
#define LIBSIXEL_PALETTE_PLAN_H

#include <sixel.h>

/*
 * Request origin and lifecycle are independent.  In particular, AUTO remains
 * the request origin after a concrete effective value has been selected.
 */
typedef enum sixel_palette_policy_origin {
    SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT = 0,
    SIXEL_PALETTE_POLICY_ORIGIN_AUTO,
    SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT,
    SIXEL_PALETTE_POLICY_ORIGIN_LEGACY_ALIAS
} sixel_palette_policy_origin_t;

typedef enum sixel_palette_policy_phase {
    SIXEL_PALETTE_POLICY_UNRESOLVED = 0,
    SIXEL_PALETTE_POLICY_RESOLVED,
    SIXEL_PALETTE_POLICY_EXECUTED,
    SIXEL_PALETTE_POLICY_BYPASSED
} sixel_palette_policy_phase_t;

typedef enum sixel_palette_resolution_reason {
    SIXEL_PALETTE_RESOLUTION_NONE = 0,
    SIXEL_PALETTE_RESOLUTION_EXPLICIT,
    SIXEL_PALETTE_RESOLUTION_LEGACY_COMPAT,
    SIXEL_PALETTE_RESOLUTION_INPUT_METADATA,
    SIXEL_PALETTE_RESOLUTION_SAMPLE_METADATA,
    SIXEL_PALETTE_RESOLUTION_QUANTIZER_CAPABILITY,
    SIXEL_PALETTE_RESOLUTION_RESOURCE_PROFILE,
    SIXEL_PALETTE_RESOLUTION_FALLBACK,
    SIXEL_PALETTE_RESOLUTION_NOT_APPLICABLE
} sixel_palette_resolution_reason_t;

/* Internal sampling names used while the legacy scheduler split is retained. */
typedef enum sixel_palette_sampling_policy {
    SIXEL_PALETTE_SAMPLING_AUTO = 0,
    SIXEL_PALETTE_SAMPLING_FULL_FRAME,
    SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID
} sixel_palette_sampling_policy_t;

typedef enum sixel_palette_sampling_source {
    SIXEL_PALETTE_SAMPLING_SOURCE_NONE = 0,
    SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME,
    SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME
} sixel_palette_sampling_source_t;

#define SIXEL_PALETTE_POLICY_VALUE_UNSET (-1)

typedef struct sixel_palette_policy_resolution {
    int requested;
    int effective;
    sixel_palette_policy_origin_t origin;
    sixel_palette_policy_phase_t phase;
    sixel_palette_resolution_reason_t reason;
} sixel_palette_policy_resolution_t;

/* Per-frame palette policy state owned by the existing encode DAG context. */
typedef struct sixel_palette_frame_state {
    sixel_palette_policy_resolution_t sampling;
    sixel_palette_sampling_source_t sampling_source;
    sixel_palette_policy_resolution_t binning;
    sixel_palette_policy_resolution_t quantizer;
} sixel_palette_frame_state_t;

SIXEL_INTERNAL_API void
sixel_palette_policy_resolution_init(
    sixel_palette_policy_resolution_t *resolution,
    int requested,
    sixel_palette_policy_origin_t origin);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_policy_resolve(
    sixel_palette_policy_resolution_t *resolution,
    int effective,
    sixel_palette_resolution_reason_t reason);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_sampling_resolve(
    sixel_palette_frame_state_t *state,
    sixel_palette_sampling_policy_t effective,
    sixel_palette_sampling_source_t source,
    sixel_palette_resolution_reason_t reason);

SIXEL_INTERNAL_API sixel_palette_sampling_policy_t
sixel_palette_sampling_select_auto(int total_threads,
                                   int heavy_operations,
                                   int async_eligible);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_sampling_resolve_auto(sixel_palette_frame_state_t *state,
                                    int total_threads,
                                    int heavy_operations,
                                    int async_eligible);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_sampling_resolve_fallback(
    sixel_palette_frame_state_t *state);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_policy_mark_executed(
    sixel_palette_policy_resolution_t *resolution);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_policy_mark_bypassed(
    sixel_palette_policy_resolution_t *resolution);

SIXEL_INTERNAL_API void
sixel_palette_frame_state_init(sixel_palette_frame_state_t *state);

#endif /* LIBSIXEL_PALETTE_PLAN_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
