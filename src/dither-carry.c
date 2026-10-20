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

#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include "dither-carry.h"
#include "dither.h"
#include "dither-common-pipeline.h"

/* The integer deadzone proof requires binary double with at least 53 bits.
 * Excess evaluation precision can keep the reference radius product wider
 * than a stored double, changing strict deadzone boundaries. Preserve the
 * original deadzone expression there, without changing the integer limiter.
 */
#if FLT_RADIX == 2 && DBL_MANT_DIG >= 53 && defined(FLT_EVAL_METHOD) && \
    (FLT_EVAL_METHOD == 0 || FLT_EVAL_METHOD == 1)
#define SIXEL_CARRY_DOUBLE_EVAL 1
#else
#define SIXEL_CARRY_DOUBLE_EVAL 0
#endif

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
sixel_carry_candidate_none(unsigned char const *source,
                            int32_t const *carry, int depth, int denominator,
                            double deadzone,
                            int32_t *candidate, unsigned char *lookup)
{
    double norm;
    double radius;
    double scale;
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
    for (n = 0; n < depth; ++n) {
        value = source[n] + (double)carry[n] * scale / denominator;
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
 * NONE retains checked arithmetic; non-RGB uses the same rational limiter.
 */
typedef struct sixel_carry_bounds {
    int64_t deadzone_limit;
    int32_t lower[256];
    int32_t upper[256];
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
    double fraction;
    int32_t rounded;
    int n;

    radius = (deadzone > 1.0e10 ? 1.0e10 : deadzone) * denominator;
    squared = radius * radius;
    /* For an exactly represented integer norm, norm < R iff norm <= ceil(R)-1.
     * Saturating above the reachable domain also avoids an out-of-range cast.
     * R=0 gives -1, so a disabled deadzone does not discard even zero carry.
     */
    bounds->deadzone_limit = squared >= 0x1p53 ? INT64_C(9007199254740992)
        : (int64_t)ceil(squared) - 1;
    /* Quantize headroom once in carry units, nearest with even half ties.
     * The nonnegative value is at most 15*255*4096, so truncation is safe.
     * No floating-point operation is needed by the soft pixel limiter.
     */
    margin = (threshold - 1.0) * 255.0 * denominator;
    rounded = (int32_t)margin;
    fraction = margin - rounded;
    rounded += fraction > 0.5 || (fraction == 0.5 && (rounded & 1));
    for (n = 0; n < 256; ++n) {
        bounds->lower[n] = -rounded - n * denominator;
        bounds->upper[n] = (255 - n) * denominator + rounded;
    }
    bounds->deadzone = deadzone;
    bounds->policy = policy;
}

static inline int32_t
sixel_carry_round(int32_t numerator, int denominator)
{
    uint32_t magnitude;
    uint32_t quotient;
    uint32_t remainder;

    /* The scale=1 soft interval lies inside [-3825,4080]. Translate by
     * the even integer 4096 before dividing: this makes every numerator
     * positive, preserves half-tie parity, and avoids signed division or
     * implementation-defined negative shifts. The largest translated value
     * is 8176*4096, well inside int32. Power-of-two denominators become
     * shifts/masks; 42 and 48 use compiler-generated constant division.
     * Non-half fractions are at least 1/48 from a half for fixed kernels;
     * double division/addition error is much smaller. Binary denominators
     * are exact, including perturbed FS (4096).
     */
    magnitude = (uint32_t)(numerator + 4096 * denominator);
    quotient = magnitude / (unsigned int)denominator;
    remainder = magnitude % (unsigned int)denominator;
    quotient += remainder * 2 > (unsigned int)denominator ||
        (remainder * 2 == (unsigned int)denominator && (quotient & 1));
    return (int32_t)quotient - 4096;
}

/* Keep the existing strict deadzone comparison on excess-precision targets
 * and for arbitrary numeric callers. This is independent of soft limiting:
 * all soft boundaries, ratios and candidate rounding remain integer-only.
 */
static int
sixel_carry_discard(int32_t const *carry, int depth, int denominator,
                    double deadzone)
{
    double norm;
    double radius;
    int n;

    norm = 0.0;
    for (n = 0; n < depth; ++n) norm += (double)carry[n] * carry[n];
    radius = (deadzone > 1.0e10 ? 1.0e10 : deadzone) * denominator;
    return norm < radius * radius;
}

/* All bounds are in [-16*255*4096,16*255*4096]. Bounded feedback has
 * |carry| <= 4080*7168 = 29245440 (including perturbed FS), because every
 * rounded candidate remains in [-3825,4080]. Thus ratio cross-products
 * and |N| <= (255*4096 + 16*255*4096)*29245440 < 2^49; Q < 2^37.
 * Even arbitrary int32 input carries fit int64 here: the corresponding
 * numerator is below 2^56. The even 4096*Q translation below is below 2^57
 * in that wider domain. It makes division unsigned, preserving negative
 * half-tie parity without relying on signed shifts or __int128.
 */
static inline void
sixel_carry_soft(sixel_carry_bounds_t const *bounds,
                 unsigned char const *source, int32_t const *carry,
                 int depth, int denominator, int32_t *candidate,
                 unsigned char *pixel)
{
    int64_t best_b;
    int64_t best_c;
    int64_t b;
    int64_t c;
    int64_t q;
    uint64_t numerator;
    uint64_t quotient;
    uint64_t remainder;
    int32_t rounded;
    int n;

    best_b = best_c = 1;
    for (n = 0; n < depth; ++n) {
        c = carry[n];
        if (c < bounds->lower[source[n]]) {
            b = -(int64_t)bounds->lower[source[n]];
            c = -c;
        } else if (c > bounds->upper[source[n]]) {
            b = bounds->upper[source[n]];
        } else {
            continue;
        }
        /* Positive denominators make cross multiplication order-preserving.
         * Keep the first minimum on ties. Equal ratios give identical exact
         * rational candidates, regardless of the selected representation.
         */
        if (b * best_c < best_b * c) {
            best_b = b;
            best_c = c;
        }
    }
    q = denominator * best_c;
    for (n = 0; n < depth; ++n) {
        numerator = (uint64_t)((source[n] + 4096) * q + carry[n] * best_b);
        quotient = numerator / (uint64_t)q;
        remainder = numerator - quotient * (uint64_t)q;
        quotient += remainder * 2 > (uint64_t)q ||
            (remainder * 2 == (uint64_t)q && (quotient & 1));
        rounded = (int32_t)quotient - 4096;
        candidate[n] = rounded;
        pixel[n] = (unsigned char)(rounded < 0 ? 0 :
                                   rounded > 255 ? 255 : rounded);
    }
}

/* HARD clips the numerator before rounding. Integer half ties match the
 * reference double path: power-of-two division is exact, and non-half fixed
 * denominator values are at least 1/48 from a half, far above double error.
 * int64 also protects arbitrary int32 numeric callers before clipping.
 */
static void
sixel_carry_hard(unsigned char const *source, int32_t const *carry,
                 int depth, int denominator, int32_t *candidate,
                 unsigned char *pixel)
{
    int64_t value;
    int n;

    for (n = 0; n < depth; ++n) {
        value = (int64_t)source[n] * denominator + carry[n];
        if (value < 0) value = 0;
        if (value > 255 * denominator) value = 255 * denominator;
        candidate[n] = pixel[n] = (unsigned char)
            sixel_carry_round((int32_t)value, denominator);
    }
}

/* Expand the common case in each scan loop. A C99 inline hint alone permits
 * the compiler to outline this repeated body, losing constant denominators.
 * Locals belong to the enclosing function; no compiler extension is needed.
 */
#define SIXEL_CARRY_CANDIDATE(DEN) \
    do { \
        norm = (int64_t)carry[0] * carry[0] + \
            (int64_t)carry[1] * carry[1] + (int64_t)carry[2] * carry[2]; \
        if (SIXEL_CARRY_DOUBLE_EVAL ? norm <= bounds->deadzone_limit : \
                sixel_carry_discard(carry, 3, DEN, bounds->deadzone)) { \
            for (n = 0; n < 3; ++n) candidate[n] = pixel[n] = source[n]; \
        } else if (bounds->policy == SIXEL_DITHER_CLAMP_HARD) { \
            for (n = 0; n < 3; ++n) { \
                rounded = source[n] * (DEN) + carry[n]; \
                if (rounded < 0) rounded = 0; \
                if (rounded > 255 * (DEN)) rounded = 255 * (DEN); \
                candidate[n] = pixel[n] = (unsigned char) \
                    sixel_carry_round(rounded, DEN); \
            } \
        } else if ( \
                carry[0] < bounds->lower[source[0]] || \
                carry[0] > bounds->upper[source[0]] || \
                carry[1] < bounds->lower[source[1]] || \
                carry[1] > bounds->upper[source[1]] || \
                carry[2] < bounds->lower[source[2]] || \
                carry[2] > bounds->upper[source[2]]) { \
            sixel_carry_soft(bounds, source, carry, 3, DEN, \
                             candidate, pixel); \
        } else { \
            for (n = 0; n < 3; ++n) { \
                rounded = sixel_carry_round(source[n] * (DEN) + carry[n], \
                                            DEN); \
                candidate[n] = rounded; \
                pixel[n] = (unsigned char)(rounded < 0 ? 0 : \
                                           rounded > 255 ? 255 : rounded); \
            } \
        } \
    } while (0)

static inline void
sixel_carry_candidate_rgb(sixel_carry_bounds_t const *bounds,
                          unsigned char const *source,
                          int32_t const *carry, int denominator,
                          int32_t *candidate, unsigned char *pixel)
{
    int64_t norm;
    int n;
    int32_t rounded;

    SIXEL_CARRY_CANDIDATE(denominator);
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
    if (depth == 3 &&
            policy != SIXEL_DITHER_CLAMP_NONE &&
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
    if (policy != SIXEL_DITHER_CLAMP_NONE) {
        if (denominator <= 0 || denominator > 4096 ||
                !(threshold >= 1.0 && threshold <= 16.0)) {
            return SIXEL_BAD_INPUT;
        }
        sixel_carry_prepare(&bounds, denominator, policy, threshold, deadzone);
        if (sixel_carry_discard(carry, depth, denominator, deadzone)) {
            for (n = 0; n < depth; ++n) candidate[n] = lookup[n] = source[n];
        } else if (policy == SIXEL_DITHER_CLAMP_HARD) {
            sixel_carry_hard(source, carry, depth, denominator,
                             candidate, lookup);
        } else {
            sixel_carry_soft(&bounds, source, carry, depth, denominator,
                             candidate, lookup);
        }
        return SIXEL_OK;
    }
    return sixel_carry_candidate_none(source, carry, depth, denominator,
        deadzone, candidate, lookup);
}


/* Compile-time stencils share the scan/keep contract but never read the tap
 * table in a pixel loop. Three spans separate both narrow edges from the
 * interior; an interior scatter has no horizontal bound test. Rotating row
 * pointers replace all ring-buffer modulo operations. The generic checked
 * implementation below remains authoritative for NONE and non-RGB depths.
 */
#define SIXEL_CARRY_TAP(DX, DY, WEIGHT, EDGE) \
    do { \
        nx = x + direction * (DX); \
        if ((DY) < remaining && (!(EDGE) || (nx >= 0 && nx < width))) { \
            destination = ring[DY] + (size_t)nx * 3; \
            destination[0] += error0 * (WEIGHT); \
            destination[1] += error1 * (WEIGHT); \
            destination[2] += error2 * (WEIGHT); \
        } \
    } while (0); \

#define SIXEL_CARRY_FS(EDGE) \
    SIXEL_CARRY_TAP(1, 0, 7, EDGE) \
    SIXEL_CARRY_TAP(-1, 1, 3, EDGE) \
    SIXEL_CARRY_TAP(0, 1, 5, EDGE) \
    SIXEL_CARRY_TAP(1, 1, 1, EDGE) \

#define SIXEL_CARRY_ATKINSON(EDGE) \
    SIXEL_CARRY_TAP(1, 0, 1, EDGE) \
    SIXEL_CARRY_TAP(2, 0, 1, EDGE) \
    SIXEL_CARRY_TAP(-1, 1, 1, EDGE) \
    SIXEL_CARRY_TAP(0, 1, 1, EDGE) \
    SIXEL_CARRY_TAP(1, 1, 1, EDGE) \
    SIXEL_CARRY_TAP(0, 2, 1, EDGE) \

#define SIXEL_CARRY_JAJUNI(EDGE) \
    SIXEL_CARRY_TAP(1, 0, 7, EDGE) \
    SIXEL_CARRY_TAP(2, 0, 5, EDGE) \
    SIXEL_CARRY_TAP(-2, 1, 3, EDGE) \
    SIXEL_CARRY_TAP(-1, 1, 5, EDGE) \
    SIXEL_CARRY_TAP(0, 1, 7, EDGE) \
    SIXEL_CARRY_TAP(1, 1, 5, EDGE) \
    SIXEL_CARRY_TAP(2, 1, 3, EDGE) \
    SIXEL_CARRY_TAP(-2, 2, 1, EDGE) \
    SIXEL_CARRY_TAP(-1, 2, 3, EDGE) \
    SIXEL_CARRY_TAP(0, 2, 5, EDGE) \
    SIXEL_CARRY_TAP(1, 2, 3, EDGE) \
    SIXEL_CARRY_TAP(2, 2, 1, EDGE) \

#define SIXEL_CARRY_STUCKI(EDGE) \
    SIXEL_CARRY_TAP(1, 0, 8, EDGE) \
    SIXEL_CARRY_TAP(2, 0, 4, EDGE) \
    SIXEL_CARRY_TAP(-2, 1, 2, EDGE) \
    SIXEL_CARRY_TAP(-1, 1, 4, EDGE) \
    SIXEL_CARRY_TAP(0, 1, 8, EDGE) \
    SIXEL_CARRY_TAP(1, 1, 4, EDGE) \
    SIXEL_CARRY_TAP(2, 1, 2, EDGE) \
    SIXEL_CARRY_TAP(-2, 2, 1, EDGE) \
    SIXEL_CARRY_TAP(-1, 2, 2, EDGE) \
    SIXEL_CARRY_TAP(0, 2, 4, EDGE) \
    SIXEL_CARRY_TAP(1, 2, 2, EDGE) \
    SIXEL_CARRY_TAP(2, 2, 1, EDGE) \

#define SIXEL_CARRY_BURKES(EDGE) \
    SIXEL_CARRY_TAP(1, 0, 8, EDGE) \
    SIXEL_CARRY_TAP(2, 0, 4, EDGE) \
    SIXEL_CARRY_TAP(-2, 1, 2, EDGE) \
    SIXEL_CARRY_TAP(-1, 1, 4, EDGE) \
    SIXEL_CARRY_TAP(0, 1, 8, EDGE) \
    SIXEL_CARRY_TAP(1, 1, 4, EDGE) \
    SIXEL_CARRY_TAP(2, 1, 2, EDGE) \

#define SIXEL_CARRY_SIERRA1(EDGE) \
    SIXEL_CARRY_TAP(1, 0, 2, EDGE) \
    SIXEL_CARRY_TAP(-1, 1, 1, EDGE) \
    SIXEL_CARRY_TAP(0, 1, 1, EDGE) \

#define SIXEL_CARRY_SIERRA2(EDGE) \
    SIXEL_CARRY_TAP(1, 0, 4, EDGE) \
    SIXEL_CARRY_TAP(2, 0, 3, EDGE) \
    SIXEL_CARRY_TAP(-2, 1, 1, EDGE) \
    SIXEL_CARRY_TAP(-1, 1, 2, EDGE) \
    SIXEL_CARRY_TAP(0, 1, 3, EDGE) \
    SIXEL_CARRY_TAP(1, 1, 2, EDGE) \
    SIXEL_CARRY_TAP(2, 1, 1, EDGE) \

#define SIXEL_CARRY_SIERRA3(EDGE) \
    SIXEL_CARRY_TAP(1, 0, 5, EDGE) \
    SIXEL_CARRY_TAP(2, 0, 3, EDGE) \
    SIXEL_CARRY_TAP(-2, 1, 2, EDGE) \
    SIXEL_CARRY_TAP(-1, 1, 4, EDGE) \
    SIXEL_CARRY_TAP(0, 1, 5, EDGE) \
    SIXEL_CARRY_TAP(1, 1, 4, EDGE) \
    SIXEL_CARRY_TAP(2, 1, 2, EDGE) \
    SIXEL_CARRY_TAP(-1, 2, 2, EDGE) \
    SIXEL_CARRY_TAP(0, 2, 3, EDGE) \
    SIXEL_CARRY_TAP(1, 2, 2, EDGE) \

#define SIXEL_CARRY_PERTURB(EDGE) \
    SIXEL_CARRY_TAP(1, 0, weights[0], EDGE) \
    SIXEL_CARRY_TAP(-1, 1, weights[1], EDGE) \
    SIXEL_CARRY_TAP(0, 1, weights[2], EDGE) \
    SIXEL_CARRY_TAP(1, 1, weights[3], EDGE) \

#define SIXEL_CARRY_SPAN(DEN, TAPS, MASK, PERTURB, EDGE, END) \
    for (; x != (END); x += direction) { \
        position = (size_t)y * width + x; \
        absolute_index = absolute_y < 0 ? 0 : \
            (size_t)absolute_y * width + x; \
        if ((MASK) && absolute_y >= 0 && \
                absolute_index < dither->pipeline_transparent_mask_size && \
                dither->pipeline_transparent_mask[absolute_index]) { \
            if (record) request->result[position] = (sixel_index_t) \
                dither->pipeline_transparent_keycolor; \
            continue; \
        } \
        source = request->data + position * 3; \
        carry = ring[0] + (size_t)x * 3; \
        SIXEL_CARRY_CANDIDATE(DEN); \
        retained = NULL; \
        keycolor = -1; \
        keep = 0; \
        if (delta) { \
            keep = sixel_dither_pipeline_6delta_try_keep_rgb888(dither, \
                absolute_index, x, absolute_y, pixel, record, \
                &retained, &keycolor); \
        } \
        index = keycolor; \
        if (!keep) { \
            index = lookup->vtbl->map_pixel_hint(lookup, pixel, hint); \
            if (index < 0 || index >= SIXEL_PALETTE_MAX) { \
                return SIXEL_BAD_INPUT; \
            } \
            hint = index; \
            if (delta) { \
                keep = sixel_dither_pipeline_6delta_try_keep_after_lookup( \
                    dither, absolute_index, x, absolute_y, pixel, \
                    request->palette + (size_t)index * 3, index, \
                    record, &retained, &keycolor); \
            } \
        } \
        if (record) request->result[position] = (sixel_index_t) \
            (keep ? keycolor : index); \
        if (keep && !diffuse) continue; \
        target = keep ? retained : request->palette + (size_t)index * 3; \
        error0 = candidate[0] - target[0]; \
        error1 = candidate[1] - target[1]; \
        error2 = candidate[2] - target[2]; \
        if (PERTURB) { \
            sixel_dither_perturb_weights(perturb, x, absolute_y, weights); \
        } \
        TAPS(EDGE) \
    } \

/* The largest soft threshold gives |error| <= 4080. Fixed incoming
 * weights sum to at most 48; independently perturbed FS weights to at most
 * 7168. Every partial sum is bounded by 4080*7168=29245440, including negative
 * contributions. Products and int32 additions therefore cannot overflow.
 * The RGB norm is <= 3*29245440^2 < 2^53. HARD has the tighter error bound 255.
 */
#define SIXEL_CARRY_DEFINE(NAME, DEN, ROWS, TAPS, MASK, PERTURB) \
static SIXELSTATUS \
sixel_carry_rgb_##NAME(sixel_dither_policy_apply_request_t const *request, \
                       int32_t *rows, sixel_carry_bounds_t const *bounds, \
                       sixel_dither_perturb_t const *perturb) \
{ \
    sixel_dither_t *dither; \
    sixel_lookup_policy_interface_t const *lookup; \
    int32_t *ring[3]; \
    int32_t const *carry; \
    unsigned char const *source; \
    int64_t norm; \
    int n; \
    int32_t rounded; \
    int32_t *recycle; \
    int32_t *destination; \
    int32_t candidate[3]; \
    unsigned char pixel[3]; \
    unsigned char const *target; \
    unsigned char const *retained; \
    size_t stride; \
    size_t position; \
    size_t absolute_index; \
    int width; \
    int y; \
    int x; \
    int nx; \
    int absolute_y; \
    int remaining; \
    int direction; \
    int stop; \
    int left; \
    int right; \
    int record; \
    int delta; \
    int diffuse; \
    int hint; \
    int index; \
    int keycolor; \
    int keep; \
    int weights[4]; \
    int32_t error0; \
    int32_t error1; \
    int32_t error2; \
 \
    dither = request->dither; \
    lookup = request->lookup_policy; \
    width = request->width; \
    stride = (size_t)width * 3; \
    ring[0] = rows; \
    ring[1] = rows + stride; \
    ring[2] = (ROWS) == 3 ? rows + stride * 2 : NULL; \
    diffuse = sixel_dither_pipeline_6delta_error_mode(dither) \
        == SIXEL_6DELTA_ERROR_DIFFUSE; \
    hint = -1; \
    for (y = 0; y < request->height; ++y) { \
        absolute_y = request->band_origin + y; \
        remaining = request->height - y; \
        record = absolute_y >= request->output_start; \
        delta = dither->pipeline_6delta_enabled && absolute_y >= 0; \
        direction = request->method_for_scan == SIXEL_SCAN_SERPENTINE \
            && (absolute_y & 1) ? -1 : 1; \
        x = direction > 0 ? 0 : width - 1; \
        stop = direction > 0 ? width : -1; \
        left = width < 4 ? stop : direction > 0 ? 2 : width - 3; \
        right = width < 4 ? stop : direction > 0 ? width - 2 : 1; \
        SIXEL_CARRY_SPAN(DEN, TAPS, MASK, PERTURB, 1, left) \
        SIXEL_CARRY_SPAN(DEN, TAPS, MASK, PERTURB, 0, right) \
        SIXEL_CARRY_SPAN(DEN, TAPS, MASK, PERTURB, 1, stop) \
        memset(ring[0], 0, stride * sizeof(*rows)); \
        recycle = ring[0]; \
        ring[0] = ring[1]; \
        if ((ROWS) == 3) ring[1] = ring[2]; \
        ring[(ROWS) - 1] = recycle; \
        if (record) sixel_dither_pipeline_row_notify(dither, absolute_y); \
    } \
    return SIXEL_OK; \
} \

SIXEL_CARRY_DEFINE(fs_0, 16, 2,
    SIXEL_CARRY_FS, 0, 0)
SIXEL_CARRY_DEFINE(fs_1, 16, 2,
    SIXEL_CARRY_FS, 1, 0)
SIXEL_CARRY_DEFINE(atkinson_0, 8, 3,
    SIXEL_CARRY_ATKINSON, 0, 0)
SIXEL_CARRY_DEFINE(atkinson_1, 8, 3,
    SIXEL_CARRY_ATKINSON, 1, 0)
SIXEL_CARRY_DEFINE(jajuni_0, 48, 3,
    SIXEL_CARRY_JAJUNI, 0, 0)
SIXEL_CARRY_DEFINE(jajuni_1, 48, 3,
    SIXEL_CARRY_JAJUNI, 1, 0)
SIXEL_CARRY_DEFINE(stucki_0, 42, 3,
    SIXEL_CARRY_STUCKI, 0, 0)
SIXEL_CARRY_DEFINE(stucki_1, 42, 3,
    SIXEL_CARRY_STUCKI, 1, 0)
SIXEL_CARRY_DEFINE(burkes_0, 32, 2,
    SIXEL_CARRY_BURKES, 0, 0)
SIXEL_CARRY_DEFINE(burkes_1, 32, 2,
    SIXEL_CARRY_BURKES, 1, 0)
SIXEL_CARRY_DEFINE(sierra1_0, 4, 2,
    SIXEL_CARRY_SIERRA1, 0, 0)
SIXEL_CARRY_DEFINE(sierra1_1, 4, 2,
    SIXEL_CARRY_SIERRA1, 1, 0)
SIXEL_CARRY_DEFINE(sierra2_0, 16, 2,
    SIXEL_CARRY_SIERRA2, 0, 0)
SIXEL_CARRY_DEFINE(sierra2_1, 16, 2,
    SIXEL_CARRY_SIERRA2, 1, 0)
SIXEL_CARRY_DEFINE(sierra3_0, 32, 3,
    SIXEL_CARRY_SIERRA3, 0, 0)
SIXEL_CARRY_DEFINE(sierra3_1, 32, 3,
    SIXEL_CARRY_SIERRA3, 1, 0)
SIXEL_CARRY_DEFINE(perturb_0, 4096, 2,
    SIXEL_CARRY_PERTURB, 0, 1)
SIXEL_CARRY_DEFINE(perturb_1, 4096, 2,
    SIXEL_CARRY_PERTURB, 1, 1)

#undef SIXEL_CARRY_CANDIDATE
#undef SIXEL_CARRY_DEFINE
#undef SIXEL_CARRY_SPAN
#undef SIXEL_CARRY_TAP
#undef SIXEL_CARRY_FS
#undef SIXEL_CARRY_ATKINSON
#undef SIXEL_CARRY_JAJUNI
#undef SIXEL_CARRY_STUCKI
#undef SIXEL_CARRY_BURKES
#undef SIXEL_CARRY_SIERRA1
#undef SIXEL_CARRY_SIERRA2
#undef SIXEL_CARRY_SIERRA3
#undef SIXEL_CARRY_PERTURB

static SIXELSTATUS
sixel_carry_rgb_dispatch(sixel_dither_policy_apply_request_t const *request,
                         int method, int32_t *rows,
                         sixel_carry_bounds_t const *bounds,
                         sixel_dither_perturb_t const *perturb)
{
    sixel_dither_t *dither;
    int mask;

    dither = request->dither;
    mask = dither->pipeline_transparent_mask != NULL &&
        dither->pipeline_transparent_keycolor >= 0 &&
        dither->pipeline_transparent_keycolor < SIXEL_PALETTE_MAX;
    if (perturb->enabled) {
        return mask
            ? sixel_carry_rgb_perturb_1(request, rows, bounds, perturb)
            : sixel_carry_rgb_perturb_0(request, rows, bounds, perturb);
    }
    switch (method) {
    case SIXEL_DIFFUSE_FS:
        return mask ? sixel_carry_rgb_fs_1(request, rows, bounds, perturb)
                    : sixel_carry_rgb_fs_0(request, rows, bounds, perturb);
    case SIXEL_DIFFUSE_ATKINSON:
        return mask
            ? sixel_carry_rgb_atkinson_1(request, rows, bounds, perturb)
            : sixel_carry_rgb_atkinson_0(request, rows, bounds, perturb);
    case SIXEL_DIFFUSE_JAJUNI:
        return mask ? sixel_carry_rgb_jajuni_1(request, rows, bounds, perturb)
                    : sixel_carry_rgb_jajuni_0(request, rows, bounds, perturb);
    case SIXEL_DIFFUSE_STUCKI:
        return mask ? sixel_carry_rgb_stucki_1(request, rows, bounds, perturb)
                    : sixel_carry_rgb_stucki_0(request, rows, bounds, perturb);
    case SIXEL_DIFFUSE_BURKES:
        return mask ? sixel_carry_rgb_burkes_1(request, rows, bounds, perturb)
                    : sixel_carry_rgb_burkes_0(request, rows, bounds, perturb);
    case SIXEL_DIFFUSE_SIERRA1:
        return mask ? sixel_carry_rgb_sierra1_1(request, rows, bounds, perturb)
                    : sixel_carry_rgb_sierra1_0(request, rows, bounds, perturb);
    case SIXEL_DIFFUSE_SIERRA2:
        return mask ? sixel_carry_rgb_sierra2_1(request, rows, bounds, perturb)
                    : sixel_carry_rgb_sierra2_0(request, rows, bounds, perturb);
    case SIXEL_DIFFUSE_SIERRA3:
        return mask ? sixel_carry_rgb_sierra3_1(request, rows, bounds, perturb)
                    : sixel_carry_rgb_sierra3_0(request, rows, bounds, perturb);
    default: return SIXEL_BAD_ARGUMENT;
    }
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
    bounded = depth == 3 &&
        dither->clamp_policy != SIXEL_DITHER_CLAMP_NONE;
    if (dither->clamp_policy != SIXEL_DITHER_CLAMP_NONE) {
        sixel_carry_prepare(&bounds, denominator, dither->clamp_policy,
            dither->clamp_threshold, dither->diffusion_deadzone);
    }
    if (bounded) {
        status = sixel_carry_rgb_dispatch(request, method, rows, &bounds,
                                          &perturb);
        goto end;
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
            if (dither->clamp_policy != SIXEL_DITHER_CLAMP_NONE) {
                if (sixel_carry_discard(current, depth, denominator,
                                        bounds.deadzone)) {
                    for (n = 0; n < depth; ++n) {
                        candidate[n] = pixel[n] =
                            request->data[position * depth + n];
                    }
                } else if (dither->clamp_policy == SIXEL_DITHER_CLAMP_HARD) {
                    sixel_carry_hard(request->data + position * depth,
                        current, depth, denominator, candidate, pixel);
                } else {
                    sixel_carry_soft(&bounds, request->data + position * depth,
                        current, depth, denominator, candidate, pixel);
                }
                status = SIXEL_OK;
            } else {
                status = sixel_carry_candidate_none(
                    request->data + position * depth, current, depth,
                    denominator, dither->diffusion_deadzone,
                    candidate, pixel);
            }
            if (SIXEL_FAILED(status)) goto end;
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
