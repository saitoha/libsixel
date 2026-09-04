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
    sixel_palette_policy_resolution_init(
        &state->binning,
        SIXEL_PALETTE_POLICY_VALUE_UNSET,
        SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT);
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
