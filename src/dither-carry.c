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
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
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

#include <limits.h>
#include <math.h>
#include <string.h>
#include "dither-carry.h"
#include "dither.h"
#include "dither-common-pipeline.h"

/* Integer numerators preserve all tap contributions until the destination
 * pixel is visited. Addition order cannot change a carry sum: there is no
 * per-tap division, rounding, or saturation. This permits a future wavefront
 * scheduler to reorder ready contributions (with race-free ownership).
 * Serpentine scan mirrors dx only; dy is always causal.
 */
static sixel_dither_carry_kernel_t const sixel_carry_kernels[] = {
    {16, 2, 4, {{1,0,7}, {-1,1,3}, {0,1,5}, {1,1,1}}},
    {8, 3, 6, {{1,0,1}, {2,0,1}, {-1,1,1}, {0,1,1}, {1,1,1}, {0,2,1}}},
    {48, 3, 12, {{1,0,7}, {2,0,5}, {-2,1,3}, {-1,1,5}, {0,1,7},
                {1,1,5}, {2,1,3}, {-2,2,1}, {-1,2,3}, {0,2,5},
                {1,2,3}, {2,2,1}}},
    {42, 3, 12, {{1,0,8}, {2,0,4}, {-2,1,2}, {-1,1,4}, {0,1,8},
                {1,1,4}, {2,1,2}, {-2,2,1}, {-1,2,2}, {0,2,4},
                {1,2,2}, {2,2,1}}},
    {32, 2, 7, {{1,0,8}, {2,0,4}, {-2,1,2}, {-1,1,4}, {0,1,8},
               {1,1,4}, {2,1,2}}},
    {4, 2, 3, {{1,0,2}, {-1,1,1}, {0,1,1}}},
    {16, 2, 7, {{1,0,4}, {2,0,3}, {-2,1,1}, {-1,1,2}, {0,1,3},
               {1,1,2}, {2,1,1}}},
    {32, 3, 10, {{1,0,5}, {2,0,3}, {-2,1,2}, {-1,1,4}, {0,1,5},
                {1,1,4}, {2,1,2}, {-1,2,2}, {0,2,3}, {1,2,2}}}
};

sixel_dither_carry_kernel_t const *
sixel_dither_carry_kernel(int method)
{
    switch (method) {
    case SIXEL_DIFFUSE_FS: return &sixel_carry_kernels[0];
    case SIXEL_DIFFUSE_ATKINSON: return &sixel_carry_kernels[1];
    case SIXEL_DIFFUSE_JAJUNI: return &sixel_carry_kernels[2];
    case SIXEL_DIFFUSE_STUCKI: return &sixel_carry_kernels[3];
    case SIXEL_DIFFUSE_BURKES: return &sixel_carry_kernels[4];
    case SIXEL_DIFFUSE_SIERRA1: return &sixel_carry_kernels[5];
    case SIXEL_DIFFUSE_SIERRA2: return &sixel_carry_kernels[6];
    case SIXEL_DIFFUSE_SIERRA3: return &sixel_carry_kernels[7];
    default: return NULL;
    }
}

static SIXELSTATUS
sixel_carry_candidate_reference(unsigned char const *source,
                            int32_t const *carry, int depth, int denominator,
                            int policy, double threshold, double deadzone,
                            int32_t *candidate, unsigned char *lookup)
{
    double norm;
    double radius;
    double scale;
    double margin;
    double delta;
    double value;
    double lower;
    double fraction;
    int n;
    int32_t rounded;

    norm = 0.0;
    for (n = 0; n < depth; ++n) {
        norm += (double)carry[n] * (double)carry[n];
    }
    /* Comparing the squared norm needs no square root. Double avoids an
     * int64 sum overflow for unbounded NONE feedback. A radius beyond the
     * maximum possible int32 vector discards every correction directly.
     */
    radius = (deadzone > 1.0e10 ? 1.0e10 : deadzone) * denominator;
    scale = norm < radius * radius ? 0.0 : 1.0;
    margin = (threshold - 1.0) * 255.0;
    if (scale != 0.0 && policy != SIXEL_DITHER_CLAMP_NONE
            && policy != SIXEL_DITHER_CLAMP_HARD) {
        /* Compare in carry units; divide only if a boundary is crossed.
         * One common scale preserves the RGB correction direction.
         */
        for (n = 0; n < depth; ++n) {
            delta = carry[n];
            if (delta * scale < (-margin - source[n]) * denominator) {
                scale = (-margin - source[n]) * denominator / delta;
            } else if (delta * scale >
                       (255.0 + margin - source[n]) * denominator) {
                scale = (255.0 + margin - source[n]) * denominator / delta;
            }
        }
    }
    for (n = 0; n < depth; ++n) {
        value = source[n] + (double)carry[n] * scale / denominator;
        if (policy == SIXEL_DITHER_CLAMP_HARD) {
            if (value < 0.0) value = 0.0;
            if (value > 255.0) value = 255.0;
        }
        /* Nearest integer, exact half ties to even, independent of the
         * floating-point rounding mode. This is symmetric for both signs;
         * unlike adding 0.5 then truncating it does not favor positive error.
         */
        lower = floor(value);
        if (lower < INT32_MIN || lower > INT32_MAX - 1) {
            return SIXEL_BAD_INPUT;
        }
        rounded = (int32_t)lower;
        fraction = value - lower;
        if (fraction > 0.5 || (fraction == 0.5 && rounded % 2 != 0)) {
            ++rounded;
        }
        candidate[n] = rounded;
        lookup[n] = (unsigned char)(rounded < 0 ? 0 :
                                   rounded > 255 ? 255 : rounded);
    }
    return SIXEL_OK;
}

/* Bounded RGB feedback never exceeds 4080 * 7168 per component, including
 * independently perturbed FS taps. Its three squared components sum to less
 * than 2^53: both the reference double norm and this int64 norm are exact.
 * NONE and other depths retain the checked reference path.
 */
typedef struct sixel_carry_bounds {
    int64_t deadzone_limit;
    int32_t lower[256];
    int32_t upper[256];
    double threshold;
    double deadzone;
    int policy;
} sixel_carry_bounds_t;

static void
sixel_carry_prepare(sixel_carry_bounds_t *bounds, int denominator,
                    int policy, double threshold, double deadzone)
{
    double radius;
    double squared;
    double margin;
    int n;

    radius = (deadzone > 1.0e10 ? 1.0e10 : deadzone) * denominator;
    squared = radius * radius;
    /* For an exactly represented integer norm, norm < R iff norm <= ceil(R)-1.
     * Saturating above the reachable domain also avoids an out-of-range cast.
     * R=0 gives -1, so a disabled deadzone does not discard even zero carry.
     */
    bounds->deadzone_limit = squared >= 0x1p53 ? INT64_C(9007199254740992)
        : (int64_t)ceil(squared) - 1;
    margin = (threshold - 1.0) * 255.0;
    for (n = 0; n < 256; ++n) {
        /* Preserve the reference's operation order before converting bounds.
         * ceil(lower) <= carry <= floor(upper) is precisely its scale=1 case.
         */
        bounds->lower[n] = (int32_t)ceil((-margin - n) * denominator);
        bounds->upper[n] = (int32_t)floor((255.0 + margin - n) * denominator);
    }
    bounds->threshold = threshold;
    bounds->deadzone = deadzone;
    bounds->policy = policy;
}

static inline int32_t
sixel_carry_round(int32_t numerator, int denominator)
{
    uint32_t magnitude;
    uint32_t quotient;
    uint32_t remainder;

    /* Work on an unsigned magnitude, avoiding implementation-defined signed
     * right shifts. Constant power-of-two denominators become shifts/masks;
     * 42 and 48 become compiler-generated constant divisions. Exact halves
     * round to even for either sign. The bounded numerator cannot be INT_MIN.
     * Non-half fractions are at least 1/48 from a half for fixed kernels;
     * double division/addition error is much smaller. Binary denominators
     * are exact, including perturbed FS (4096).
     */
    magnitude = (uint32_t)(numerator < 0 ? -numerator : numerator);
    quotient = magnitude / (unsigned int)denominator;
    remainder = magnitude % (unsigned int)denominator;
    quotient += remainder * 2 > (unsigned int)denominator ||
        (remainder * 2 == (unsigned int)denominator && (quotient & 1));
    return numerator < 0 ? -(int32_t)quotient : (int32_t)quotient;
}

static inline void
sixel_carry_candidate_rgb(sixel_carry_bounds_t const *bounds,
                          unsigned char const *source,
                          int32_t const *carry, int denominator,
                          int32_t *candidate, unsigned char *pixel)
{
    int64_t norm;
    int n;
    int32_t rounded;

    norm = (int64_t)carry[0] * carry[0] + (int64_t)carry[1] * carry[1]
        + (int64_t)carry[2] * carry[2];
    if (norm <= bounds->deadzone_limit) {
        for (n = 0; n < 3; ++n) candidate[n] = pixel[n] = source[n];
        return;
    }
    if (bounds->policy == SIXEL_DITHER_CLAMP_HARD ||
            carry[0] < bounds->lower[source[0]] ||
            carry[0] > bounds->upper[source[0]] ||
            carry[1] < bounds->lower[source[1]] ||
            carry[1] > bounds->upper[source[1]] ||
            carry[2] < bounds->lower[source[2]] ||
            carry[2] > bounds->upper[source[2]]) {
        /* Only a clipped vector needs the original common-scale double
         * arithmetic. Bounded candidates cannot fail the int32 range check.
         */
        (void)sixel_carry_candidate_reference(source, carry, 3, denominator,
            bounds->policy, bounds->threshold, 0.0, candidate, pixel);
        return;
    }
    for (n = 0; n < 3; ++n) {
        rounded = sixel_carry_round(source[n] * denominator + carry[n],
                                    denominator);
        candidate[n] = rounded;
        pixel[n] = (unsigned char)(rounded < 0 ? 0 :
                                   rounded > 255 ? 255 : rounded);
    }
}

SIXELSTATUS
sixel_dither_carry_candidate(unsigned char const *source,
                            int32_t const *carry, int depth, int denominator,
                            int policy, double threshold, double deadzone,
                            int32_t *candidate, unsigned char *lookup)
{
    sixel_carry_bounds_t bounds;
    int n;

    /* This entry point also serves numeric callers with arbitrary carries.
     * The production RGB loop proves this bound inductively and prepares
     * its state once per band instead of testing it for every candidate.
     */
    if (depth == 3 && policy != SIXEL_DITHER_CLAMP_NONE &&
            threshold >= 1.0 && threshold <= 16.0 &&
            denominator > 0 && denominator <= 4096) {
        for (n = 0; n < 3; ++n) {
            if (carry[n] < -29245440 || carry[n] > 29245440) break;
        }
        if (n == 3) {
            sixel_carry_prepare(&bounds, denominator, policy, threshold,
                                 deadzone);
            sixel_carry_candidate_rgb(&bounds, source, carry, denominator,
                                      candidate, lookup);
            return SIXEL_OK;
        }
    }
    return sixel_carry_candidate_reference(source, carry, depth, denominator,
        policy, threshold, deadzone, candidate, lookup);
}

SIXELSTATUS
sixel_dither_carry_apply(sixel_dither_policy_apply_request_t const *request,
                        int method)
{
    sixel_dither_carry_kernel_t const *kernel;
    sixel_dither_t *dither;
    sixel_lookup_policy_interface_t const *lookup;
    sixel_dither_perturb_t perturb;
    sixel_carry_bounds_t bounds;
    int bounded;
    int32_t *rows;
    int32_t *current;
    int32_t *destination;
    int32_t candidate[SIXEL_MAX_CHANNELS];
    unsigned char pixel[SIXEL_MAX_CHANNELS];
    unsigned char const *target;
    unsigned char const *retained;
    size_t stride;
    size_t position;
    size_t absolute_index;
    int width;
    int depth;
    int y;
    int x;
    int absolute_y;
    int direction;
    int stop;
    int n;
    int tap;
    int nx;
    int ny;
    int hint;
    int index;
    int keycolor;
    int keep;
    int record;
    int denominator;
    int weights[4];
    int weight;
    int64_t sum;
    SIXELSTATUS status;

    if (request == NULL || request->dither == NULL
            || request->data == NULL || request->palette == NULL
            || request->result == NULL || request->width <= 0
            || request->height <= 0 || request->depth <= 0
            || request->depth > SIXEL_MAX_CHANNELS) {
        return SIXEL_BAD_ARGUMENT;
    }
    kernel = sixel_dither_carry_kernel(method);
    lookup = request->lookup_policy;
    if (kernel == NULL || lookup == NULL || lookup->vtbl == NULL
            || lookup->vtbl->map_pixel_hint == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    dither = request->dither;
    width = request->width;
    depth = request->depth;
    stride = (size_t)width * (size_t)depth;
    if (stride / (size_t)depth != (size_t)width
            || stride > SIZE_MAX / sizeof(*rows) / kernel->rows) {
        return SIXEL_BAD_ALLOCATION;
    }
    /* Allocate once per band. No state is shared across overlapping bands;
     * the caller supplies warm-up rows and an absolute output boundary.
     * Allocation failure is an error, never an output-changing fallback.
     */
    rows = sixel_allocator_calloc(dither->allocator,
                                  stride * kernel->rows, sizeof(*rows));
    if (rows == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_dither_perturb_init(&perturb, dither->diffusion_perturb,
                              dither->diffusion_perturb_seed);
    perturb.enabled = perturb.enabled && method == SIXEL_DIFFUSE_FS;
    denominator = perturb.enabled ? 16 << 8 : kernel->denominator;
    bounded = depth == 3 && dither->clamp_policy != SIXEL_DITHER_CLAMP_NONE;
    if (bounded) {
        sixel_carry_prepare(&bounds, denominator, dither->clamp_policy,
            dither->clamp_threshold, dither->diffusion_deadzone);
    }
    hint = -1;
    status = SIXEL_OK;
    for (y = 0; y < request->height; ++y) {
        absolute_y = request->band_origin + y;
        direction = request->method_for_scan == SIXEL_SCAN_SERPENTINE
            && (absolute_y & 1) ? -1 : 1;
        x = direction > 0 ? 0 : width - 1;
        stop = direction > 0 ? width : -1;
        for (; x != stop; x += direction) {
            position = (size_t)y * width + x;
            absolute_index = absolute_y < 0 ? 0 :
                (size_t)absolute_y * width + x;
            record = absolute_y >= request->output_start;
            current = rows + (size_t)(y % kernel->rows) * stride
                + (size_t)x * depth;
            /* Transparent destinations absorb incoming correction. Taps may
             * jump over a transparent pixel as in the existing stencil;
             * the masked pixel never looks up or relays a residual.
             */
            if (absolute_y >= 0 && dither->pipeline_transparent_mask != NULL
                    && dither->pipeline_transparent_keycolor >= 0
                    && dither->pipeline_transparent_keycolor < SIXEL_PALETTE_MAX
                    && absolute_index < dither->pipeline_transparent_mask_size
                    && dither->pipeline_transparent_mask[absolute_index]) {
                if (record) {
                    request->result[position] = (sixel_index_t)
                        dither->pipeline_transparent_keycolor;
                }
                continue;
            }
            if (bounded) {
                sixel_carry_candidate_rgb(&bounds,
                    request->data + position * 3, current, denominator,
                    candidate, pixel);
            } else {
                status = sixel_carry_candidate_reference(
                    request->data + position * depth, current, depth,
                    denominator, dither->clamp_policy,
                    dither->clamp_threshold, dither->diffusion_deadzone,
                    candidate, pixel);
                if (SIXEL_FAILED(status)) goto end;
            }
            retained = NULL;
            keycolor = -1;
            keep = 0;
            if (absolute_y >= 0 && depth >= 3) {
                keep = sixel_dither_pipeline_6delta_try_keep_rgb888(
                    dither, absolute_index, x, absolute_y, pixel, record,
                    &retained, &keycolor);
            }
            index = keycolor;
            if (!keep) {
                index = lookup->vtbl->map_pixel_hint(lookup, pixel, hint);
                if (index < 0 || index >= SIXEL_PALETTE_MAX) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                hint = index;
                if (absolute_y >= 0 && depth >= 3) {
                    keep = sixel_dither_pipeline_6delta_try_keep_after_lookup(
                        dither, absolute_index, x, absolute_y, pixel,
                        request->palette + (size_t)index * depth, index,
                        record, &retained, &keycolor);
                }
            }
            if (record) {
                request->result[position] = (sixel_index_t)
                    (keep ? keycolor : index);
            }
            if (keep && sixel_dither_pipeline_6delta_error_mode(dither)
                    != SIXEL_6DELTA_ERROR_DIFFUSE) {
                continue;
            }
            target = keep ? retained : request->palette + (size_t)index * depth;
            if (perturb.enabled) {
                sixel_dither_perturb_weights(&perturb, x, absolute_y, weights);
            }
            for (tap = 0; tap < kernel->count; ++tap) {
                nx = x + direction * kernel->taps[tap].dx;
                ny = y + kernel->taps[tap].dy;
                if (nx < 0 || nx >= width || ny >= request->height) continue;
                destination = rows + (size_t)(ny % kernel->rows) * stride
                    + (size_t)nx * depth;
                weight = perturb.enabled ? weights[tap]
                    : kernel->taps[tap].weight;
                for (n = 0; n < depth; ++n) {
                    /* At default soft bounds |error| <= 258. Fixed kernels
                     * sum to at most 48; perturbed FS incoming weights sum
                     * to at most 28*256 even with independently varied taps.
                     * Thus |carry| <= 258*7168 = 1849344 < INT32_MAX.
                     * Even threshold=16 is safe (4080*7168 < INT32_MAX).
                     * NONE has no such bound: check widened arithmetic and
                     * return an error if feedback exceeds int32 capacity.
                     */
                    sum = destination[n] +
                        ((int64_t)candidate[n] - target[n]) * weight;
                    if (sum < INT32_MIN || sum > INT32_MAX) {
                        status = SIXEL_BAD_INPUT;
                        goto end;
                    }
                    destination[n] = (int32_t)sum;
                }
            }
        }
        memset(rows + (size_t)(y % kernel->rows) * stride, 0,
               stride * sizeof(*rows));
        if (absolute_y >= request->output_start) {
            sixel_dither_pipeline_row_notify(dither, absolute_y);
        }
    }
end:
    sixel_allocator_free(dither->allocator, rows);
    return status;
}
