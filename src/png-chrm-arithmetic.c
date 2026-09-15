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
#include "png-chrm-arithmetic.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

/* The target definition is fixed independently of the arithmetic mode.
 * The private test build can retain the previous decimal target as a control.
 * This compile-time control is not a loader or public API option.
 */
#ifndef PNG_CHRM_STANDARD_TARGET
#define PNG_CHRM_STANDARD_TARGET 1
#endif

#define PNG_CHRM_WORDS 16

/* Integer arithmetic below has a fixed, proved bound for this domain:
 * PNG xy grid [0,100000], positive y, x+y<=100000; nonzero primary area;
 * nonzero required Bradford cones; binary32 input [0,1]. Decimal/standard
 * coefficient numerators need at most 232/228 bits, denominators 230/225,
 * input-weighted sums 383/378, midpoint comparisons 453/448. All are <512.
 * Assertions verify these invariants; no invalid caller value reaches them.
 */
static int64_t const png_chrm_bradford[3][3] = {
    {8951, 2664, -1614}, {-7502, 17135, 367}, {389, -685, 10296}};
static int64_t const png_chrm_adjugate[3][3] = {
    {176673355, -26322954, 28633578},
    {77383355, 92787342, 8823211},
    {-1526645, 7167731, 173360713}};
#if PNG_CHRM_STANDARD_TARGET
#define PNG_CHRM_TARGET_DEN 44092012995670LL
static int64_t const png_chrm_target[3][3] = {
    {142900888797030LL, -67786319044745LL, -21984752122620LL},
    {-42735903006867LL, 82715183455933LL, 1832246131233LL},
    {2452842196950LL, -8993754722150LL, 46604001742050LL}};
#else
#define PNG_CHRM_TARGET_DEN 1000000000000000LL
static int64_t const png_chrm_target[3][3] = {
    {3240969941904521LL, -1537383177570093LL, -498610760293003LL},
    {-969243636280880LL, 1875967501507721LL, 41555057407176LL},
    {55630079696993LL, -203976958888977LL, 1056971514242878LL}};
#endif
static int64_t const png_chrm_d65_cone[3] = {309713750, 342303570, 358533210};

static int
png_chrm_used(sixel_png_chrm_integer_t a)
{
    int i;

    for (i = PNG_CHRM_WORDS; i > 0 && a.w[i - 1] == 0; --i) {
    }
    return i;
}

static sixel_png_chrm_integer_t
png_chrm_integer(int64_t a)
{
    sixel_png_chrm_integer_t r;
    uint64_t magnitude;

    memset(&r, 0, sizeof(r));
    assert(a != INT64_MIN);
    magnitude = a < 0 ? (uint64_t)-a : (uint64_t)a;
    r.w[0] = (uint32_t)magnitude;
    r.w[1] = (uint32_t)(magnitude >> 32);
    r.sign = (a > 0) - (a < 0);
    return r;
}

static int
png_chrm_magnitude_compare(sixel_png_chrm_integer_t a,
                           sixel_png_chrm_integer_t b)
{
    int i;

    for (i = PNG_CHRM_WORDS - 1; i >= 0; --i) {
        if (a.w[i] != b.w[i]) {
            return a.w[i] > b.w[i] ? 1 : -1;
        }
    }
    return 0;
}

static sixel_png_chrm_integer_t
png_chrm_plus(sixel_png_chrm_integer_t a, sixel_png_chrm_integer_t b)
{
    sixel_png_chrm_integer_t r;
    sixel_png_chrm_integer_t t;
    uint64_t carry;
    uint64_t value;
    int i;
    int cmp;

    if (!a.sign) {
        return b;
    }
    if (!b.sign) {
        return a;
    }
    r = png_chrm_integer(0);
    carry = 0;
    if (a.sign == b.sign) {
        r.sign = a.sign;
        for (i = 0; i < PNG_CHRM_WORDS; ++i) {
            value = (uint64_t)a.w[i] + b.w[i] + carry;
            r.w[i] = (uint32_t)value;
            carry = value >> 32;
        }
        assert(carry == 0);
        return r;
    }
    cmp = png_chrm_magnitude_compare(a, b);
    if (!cmp) {
        return r;
    }
    if (cmp < 0) {
        t = a;
        a = b;
        b = t;
    }
    r.sign = a.sign;
    for (i = 0; i < PNG_CHRM_WORDS; ++i) {
        value = (uint64_t)b.w[i] + carry;
        carry = (uint64_t)a.w[i] < value;
        r.w[i] = (uint32_t)((uint64_t)a.w[i] - value);
    }
    assert(carry == 0);
    return r;
}

static sixel_png_chrm_integer_t
png_chrm_times(sixel_png_chrm_integer_t a, sixel_png_chrm_integer_t b)
{
    sixel_png_chrm_integer_t r;
    uint64_t carry;
    uint64_t value;
    int i;
    int j;
    int na;
    int nb;

    r = png_chrm_integer(0);
    na = png_chrm_used(a);
    nb = png_chrm_used(b);
    assert(na + nb <= PNG_CHRM_WORDS);
    for (i = 0; i < na; ++i) {
        carry = 0;
        for (j = 0; j < nb; ++j) {
            value = (uint64_t)a.w[i] * b.w[j] + r.w[i + j] + carry;
            r.w[i + j] = (uint32_t)value;
            carry = value >> 32;
        }
        r.w[i + nb] = (uint32_t)carry;
    }
    r.sign = na && nb ? a.sign * b.sign : 0;
    return r;
}

static sixel_png_chrm_integer_t
png_chrm_shift(sixel_png_chrm_integer_t a, int bits)
{
    sixel_png_chrm_integer_t r;
    uint64_t value;
    int i;
    int index;

    r = png_chrm_integer(0);
    assert(bits >= 0);
    for (i = 0; i < png_chrm_used(a); ++i) {
        index = i + bits / 32;
        assert(index < PNG_CHRM_WORDS);
        value = (uint64_t)a.w[i] << (bits % 32);
        r.w[index] |= (uint32_t)value;
        if (value >> 32) {
            assert(index + 1 < PNG_CHRM_WORDS);
            r.w[index + 1] |= (uint32_t)(value >> 32);
        }
    }
    r.sign = a.sign;
    return r;
}

/* Decode a nonnegative binary32 into integer units of 2^-149. */
static sixel_png_chrm_integer_t
png_chrm_float_units(uint32_t bits)
{
    uint32_t exponent;
    uint32_t significand;

    assert(bits < 0x7f800000u);
    exponent = bits >> 23;
    significand = bits & 0x7fffffu;
    if (exponent) {
        significand |= 0x800000u;
        return png_chrm_shift(png_chrm_integer(significand), (int)exponent - 1);
    }
    return png_chrm_integer(significand);
}

/* Exact rational rounding. n / (d * 2^149) has magnitude below 2^72. */
static uint32_t
png_chrm_round_exact_search(sixel_png_chrm_integer_t n,
                            sixel_png_chrm_integer_t d)
{
    uint32_t low;
    uint32_t high;
    uint32_t mid;
    uint32_t sign;
    int cmp;
    sixel_png_chrm_integer_t rhs;

    assert(d.sign > 0);
    if (!n.sign) {
        return 0;
    }
    sign = n.sign < 0 ? 0x80000000u : 0;
    low = 0;
    high = (127u + 72u) << 23;
    rhs = png_chrm_times(d, png_chrm_float_units(high));
    assert(png_chrm_magnitude_compare(n, rhs) < 0);
    while (high - low > 1u) {
        mid = low + (high - low) / 2u;
        rhs = png_chrm_times(d, png_chrm_float_units(mid));
        if (png_chrm_magnitude_compare(n, rhs) >= 0) {
            low = mid;
        } else {
            high = mid;
        }
    }
    rhs = png_chrm_times(d, png_chrm_plus(png_chrm_float_units(low),
                                          png_chrm_float_units(high)));
    cmp = png_chrm_magnitude_compare(png_chrm_shift(n, 1), rhs);
    if (cmp > 0 || (cmp == 0 && (low & 1u))) {
        low = high;
    }
    return low | sign;
}

/* Use leading bits only as a hint; exact cell comparisons decide acceptance. */
static double
png_chrm_leading_double(sixel_png_chrm_integer_t a, int *exponent)
{
    uint64_t value;
    uint32_t high;
    int width;
    int words;
    int shift_bits;
    int index;
    int offset;

    words = png_chrm_used(a);
    assert(words > 0);
    high = a.w[words - 1];
    width = (words - 1) * 32;
    while (high) {
        ++width;
        high >>= 1;
    }
    shift_bits = width > 53 ? width - 53 : 0;
    index = shift_bits / 32;
    offset = shift_bits % 32;
    value = (uint64_t)a.w[index] >> offset;
    if (index + 1 < PNG_CHRM_WORDS) {
        value |= (uint64_t)a.w[index + 1] << (32 - offset);
    }
    if (offset && index + 2 < PNG_CHRM_WORDS) {
        value |= (uint64_t)a.w[index + 2] << (64 - offset);
    }
    assert(value <= (UINT64_C(1) << 53));
    *exponent = shift_bits;
    return (double)value;
}

/* -1/0/+1 means below/inside/above the candidate's round-to-even cell. */
static int
png_chrm_exact_cell(sixel_png_chrm_integer_t n, sixel_png_chrm_integer_t d,
                    uint32_t candidate)
{
    sixel_png_chrm_integer_t twice;
    sixel_png_chrm_integer_t rhs;
    int cmp;
    int odd;

    twice = png_chrm_shift(n, 1);
    odd = (candidate & 1u) != 0;
    if (candidate) {
        rhs =
            png_chrm_times(d, png_chrm_plus(png_chrm_float_units(candidate - 1),
                                            png_chrm_float_units(candidate)));
        cmp = png_chrm_magnitude_compare(twice, rhs);
        if (cmp < 0 || (!cmp && odd)) {
            return -1;
        }
    }
    rhs = png_chrm_times(d, png_chrm_plus(png_chrm_float_units(candidate),
                                          png_chrm_float_units(candidate + 1)));
    cmp = png_chrm_magnitude_compare(twice, rhs);
    return cmp > 0 || (!cmp && odd) ? 1 : 0;
}

static uint32_t
png_chrm_round_exact(sixel_png_chrm_integer_t n, sixel_png_chrm_integer_t d)
{
    double a;
    double b;
    double hint;
    float rounded;
    uint32_t candidate;
    uint32_t sign;
    int na;
    int nb;
    int direction;

    assert(d.sign > 0);
    if (!n.sign) {
        return 0;
    }
    sign = n.sign < 0 ? 0x80000000u : 0;
    a = png_chrm_leading_double(n, &na);
    b = png_chrm_leading_double(d, &nb);
    hint = ldexp(a / b, na - nb - 149);
    rounded = (float)hint;
    memcpy(&candidate, &rounded, sizeof(candidate));
    if (candidate <= ((127u + 72u) << 23)) {
        direction = png_chrm_exact_cell(n, d, candidate);
        if (!direction) {
            return candidate | sign;
        }
        if ((direction > 0 && candidate < ((127u + 72u) << 23)) ||
            (direction < 0 && candidate > 0)) {
            candidate = direction > 0 ? candidate + 1 : candidate - 1;
            if (!png_chrm_exact_cell(n, d, candidate)) {
                return candidate | sign;
            }
        }
    }
    return png_chrm_round_exact_search(n, d);
}

static int64_t
png_chrm_area(int64_t const *p, int64_t const *q, int64_t const *r)
{
    return (p[0] - r[0]) * (q[1] - r[1]) - (q[0] - r[0]) * (p[1] - r[1]);
}

static void
png_chrm_geometry(sixel_png_chrm_matrix_t *m, int64_t *a, int64_t weights[3],
                  int64_t points[3][3], int64_t q[3])
{
    int i;
    int j;

    *a = png_chrm_area(m->grid + 2, m->grid + 4, m->grid + 6);
    weights[0] = png_chrm_area(m->grid, m->grid + 4, m->grid + 6);
    weights[1] = png_chrm_area(m->grid + 2, m->grid, m->grid + 6);
    weights[2] = png_chrm_area(m->grid + 2, m->grid + 4, m->grid);
    assert(weights[0] + weights[1] + weights[2] == *a);
    for (i = 0; i < 3; ++i) {
        points[i][0] = m->grid[2 + 2 * i];
        points[i][1] = m->grid[3 + 2 * i];
        points[i][2] = 100000 - points[i][0] - points[i][1];
        q[i] = png_chrm_bradford[i][0] * m->grid[0] +
               png_chrm_bradford[i][1] * m->grid[1] +
               png_chrm_bradford[i][2] * (100000 - m->grid[0] - m->grid[1]);
        for (j = 0; j < 3; ++j) {
            assert(points[i][j] >= 0);
        }
    }
}

static void
png_chrm_exact_build(sixel_png_chrm_matrix_t *m)
{
    int64_t a;
    int64_t weights[3];
    int64_t points[3][3];
    int64_t q[3];
    int64_t cone;
    sixel_png_chrm_integer_t term;
    sixel_png_chrm_integer_t sum;
    int r;
    int c;
    int j;
    int k;

    png_chrm_geometry(m, &a, weights, points, q);
    m->d = png_chrm_times(png_chrm_integer(a),
                          png_chrm_integer(PNG_CHRM_TARGET_DEN));
    if (m->cat) {
        m->d = png_chrm_times(m->d, png_chrm_integer(32900));
        m->d = png_chrm_times(m->d, png_chrm_integer(1790016463355LL));
        for (k = 0; k < 3; ++k) {
            m->d = png_chrm_times(m->d, png_chrm_integer(q[k]));
        }
    } else {
        m->d = png_chrm_times(m->d, png_chrm_integer(m->grid[1]));
    }
    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 3; ++c) {
            sum = png_chrm_integer(0);
            for (j = 0; j < 3; ++j) {
                if (!m->cat) {
                    term =
                        png_chrm_times(png_chrm_integer(weights[c]),
                                       png_chrm_integer(png_chrm_target[r][j]));
                    term = png_chrm_times(term, png_chrm_integer(points[c][j]));
                    sum = png_chrm_plus(sum, term);
                    continue;
                }
                for (k = 0; k < 3; ++k) {
                    cone = png_chrm_bradford[k][0] * points[c][0] +
                           png_chrm_bradford[k][1] * points[c][1] +
                           png_chrm_bradford[k][2] * points[c][2];
                    term =
                        png_chrm_times(png_chrm_integer(weights[c]),
                                       png_chrm_integer(png_chrm_target[r][j]));
                    term = png_chrm_times(
                        term, png_chrm_integer(png_chrm_adjugate[j][k]));
                    term = png_chrm_times(
                        term, png_chrm_integer(png_chrm_d65_cone[k]));
                    term = png_chrm_times(term, png_chrm_integer(cone));
                    term =
                        png_chrm_times(term, png_chrm_integer(q[(k + 1) % 3]));
                    term =
                        png_chrm_times(term, png_chrm_integer(q[(k + 2) % 3]));
                    sum = png_chrm_plus(sum, term);
                }
            }
            m->n[3 * r + c] = sum;
        }
    }
    if (m->d.sign < 0) {
        m->d.sign = 1;
        for (j = 0; j < 9; ++j) {
            m->n[j].sign *= -1;
        }
    }
    m->exact_ready = 1;
}

/* The original-coordinate measures have meaningful, fixed units. Comparing
 * an arbitrary common rational denominator would change with representation.
 * The 1e-5 thresholds are routing policies, not accuracy bounds.
 * Exact equality stays in double. Exact zero is handled before dispatch.
 */
static unsigned
png_chrm_choose_reason(int64_t a, int64_t const q[3], int64_t white_y, int cat)
{
    unsigned reason;
    int k;

    reason = 0u;
    if (a < 0) {
        a = -a;
    }
    if (a < 100000) {
        reason |= SIXEL_PNG_CHRM_SMALL_AREA;
    }
    if (cat) {
        for (k = 0; k < 3; ++k) {
            if (10 * (q[k] < 0 ? -q[k] : q[k]) < white_y) {
                reason |= SIXEL_PNG_CHRM_SMALL_CONE;
            }
        }
    }
    return reason ? reason : SIXEL_PNG_CHRM_ORDINARY;
}

static int
png_chrm_supports_binary32(void)
{
    float one;
    uint32_t encoded;

    if (sizeof(float) != 4 || FLT_RADIX != 2 || FLT_MANT_DIG != 24 ||
        FLT_MAX_EXP != 128 || FLT_MIN_EXP != -125) {
        return 0;
    }
    one = 1.0f;
    memcpy(&encoded, &one, sizeof(encoded));
    return encoded == UINT32_C(0x3f800000);
}

/* Construct source columns directly from barycentric area ratios. The
 * integer numerators avoid cancellation in a floating inverse of the xy
 * primaries. CAT denominators are also formed from the original integers.
 * This path performs no 512bit operations and builds no error enclosure.
 */
static void
png_chrm_double_build(sixel_png_chrm_matrix_t *m, int64_t a,
                      int64_t const weights[3], int64_t points[3][3],
                      int64_t const q[3])
{
    double target[3][3];
    double src[3][3];
    double adapted[3][3];
    double ratio[3];
    double cone;
    double sum;
    int r;
    int c;
    int j;
    int k;

    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 3; ++c) {
            target[r][c] =
                (double)png_chrm_target[r][c] / (double)PNG_CHRM_TARGET_DEN;
            src[r][c] =
                (double)(weights[c] * points[c][r]) / (double)(a * m->grid[1]);
        }
    }
    if (m->cat) {
        for (k = 0; k < 3; ++k) {
            ratio[k] = (double)(png_chrm_d65_cone[k] * m->grid[1]) /
                       (double)(32900 * q[k]);
        }
        for (r = 0; r < 3; ++r) {
            for (c = 0; c < 3; ++c) {
                sum = 0.0;
                for (k = 0; k < 3; ++k) {
                    cone = 0.0;
                    for (j = 0; j < 3; ++j) {
                        cone += (double)png_chrm_bradford[k][j] * src[j][c];
                    }
                    sum += (double)png_chrm_adjugate[r][k] / 1790016463355.0 *
                           ratio[k] * cone;
                }
                adapted[r][c] = sum;
            }
        }
    } else {
        memcpy(adapted, src, sizeof(src));
    }
    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 3; ++c) {
            sum = 0.0;
            for (j = 0; j < 3; ++j) {
                sum += target[r][j] * adapted[j][c];
            }
            m->m[3 * r + c] = sum;
        }
    }

}

int
sixel_png_chrm_build(sixel_png_chrm_matrix_t *out, int64_t const grid[8],
                     int requested, int adapt)
{
    sixel_png_chrm_matrix_t temp;
    int64_t a;
    int64_t weights[3];
    int64_t points[3][3];
    int64_t q[3];
    int i;
    static int64_t const srgb[8] = {31270, 32900, 64000, 33000,
                                    30000, 60000, 15000, 6000};

    if (out == NULL || grid == NULL || requested < SIXEL_PNG_CHRM_AUTO ||
        requested > SIXEL_PNG_CHRM_512BIT) {
        return SIXEL_PNG_CHRM_INVALID_ARGUMENT;
    }
    for (i = 0; i < 8; i += 2) {
        if (grid[i] < 0 || grid[i] > 100000 || grid[i + 1] <= 0 ||
            grid[i + 1] > 100000 || grid[i] + grid[i + 1] > 100000) {
            return SIXEL_PNG_CHRM_METADATA_SKIP;
        }
    }
    memset(&temp, 0, sizeof(temp));
    memcpy(temp.grid, grid, sizeof(temp.grid));
    temp.cat = adapt && (grid[0] != 31270 || grid[1] != 32900);
    png_chrm_geometry(&temp, &a, weights, points, q);
    if (!a) {
        return SIXEL_PNG_CHRM_METADATA_SKIP;
    }
    if (temp.cat && (!q[0] || !q[1] || !q[2])) {
        return SIXEL_PNG_CHRM_METADATA_SKIP;
    }
    temp.requested = requested;
    temp.reason = png_chrm_choose_reason(a, q, grid[1], temp.cat);
    temp.selected = requested;
    if (requested == SIXEL_PNG_CHRM_AUTO) {
        temp.selected = temp.reason == SIXEL_PNG_CHRM_ORDINARY
                            ? SIXEL_PNG_CHRM_DOUBLE
                            : SIXEL_PNG_CHRM_512BIT;
    } else {
        temp.reason = SIXEL_PNG_CHRM_FORCED;
    }
    temp.identity =
        PNG_CHRM_STANDARD_TARGET && memcmp(grid, srgb, sizeof(srgb)) == 0;
    temp.gray = temp.cat || (grid[0] == 31270 && grid[1] == 32900);
    if (temp.selected == SIXEL_PNG_CHRM_512BIT) {
        if (!png_chrm_supports_binary32()) {
            return SIXEL_PNG_CHRM_UNSUPPORTED;
        }
        png_chrm_exact_build(&temp);
    } else {
        png_chrm_double_build(&temp, a, weights, points, q);
    }
    *out = temp;
    return SIXEL_PNG_CHRM_OK;
}

int
sixel_png_chrm_apply(sixel_png_chrm_matrix_t const *m, float const *input,
                     float *output, size_t count, sixel_png_chrm_stats_t *stats)
{
    sixel_png_chrm_stats_t counts;
    float x[3];
    float result[3];
    uint32_t encoded[3];
    uint32_t rounded;
    sixel_png_chrm_integer_t units[3];
    sixel_png_chrm_integer_t total;
    size_t i;
    int r;
    int c;

    if (m == NULL || input == NULL || output == NULL || count > SIZE_MAX / 3 ||
        (m->selected != SIXEL_PNG_CHRM_DOUBLE &&
         m->selected != SIXEL_PNG_CHRM_512BIT)) {
        return SIXEL_PNG_CHRM_INVALID_ARGUMENT;
    }
    if (m->selected == SIXEL_PNG_CHRM_512BIT && !m->exact_ready) {
        return SIXEL_PNG_CHRM_INVALID_ARGUMENT;
    }
    /* Preflight every component before writing. Ordered comparisons reject
     * NaN/infinity as well as inputs outside this bounded PNG proof domain.
     * This validation is not a per-pixel error estimate or mode selection.
     */
    for (i = 0; i < count * 3; ++i) {
        if (m->selected == SIXEL_PNG_CHRM_512BIT) {
            /* Inspect bits so flush-to-zero cannot disguise subnormal
             * values or make unequal near-black channels look neutral. */
            memcpy(&rounded, input + i, sizeof(rounded));
            if ((rounded & UINT32_C(0x7fffffff)) > UINT32_C(0x3f800000) ||
                (rounded > UINT32_C(0x80000000))) {
                return SIXEL_PNG_CHRM_INVALID_PIXELS;
            }
        } else if (!(input[i] >= 0.0f && input[i] <= 1.0f)) {
            return SIXEL_PNG_CHRM_INVALID_PIXELS;
        }
    }
    memset(&counts, 0, sizeof(counts));
    for (i = 0; i < count; ++i) {
        memcpy(x, input + 3 * i, sizeof(x));
        if (m->selected == SIXEL_PNG_CHRM_512BIT) {
            memcpy(encoded, x, sizeof(encoded));
            for (c = 0; c < 3; ++c) {
                encoded[c] &= UINT32_C(0x7fffffff);
            }
        }
        if (m->selected == SIXEL_PNG_CHRM_512BIT &&
            (m->identity || (m->gray && encoded[0] == encoded[1] &&
                            encoded[1] == encoded[2]))) {
            memcpy(result, encoded, sizeof(result));
            ++counts.identity_pixels;
        } else if (m->selected == SIXEL_PNG_CHRM_DOUBLE &&
                   (m->identity || (m->gray && x[0] == x[1] &&
                                   x[1] == x[2]))) {
            /* Standard sRGB preserves the real value. For the decimal
             * target the exact gray multiplier differs from one by <2^-25,
             * proving that every allowed binary32 gray rounds to itself.
             * Only equal channels qualify; no near-gray epsilon is used.
             */
            for (r = 0; r < 3; ++r) {
                result[r] = x[r] == 0.0f ? 0.0f : x[r];
            }
            ++counts.identity_pixels;
        } else if (m->selected == SIXEL_PNG_CHRM_DOUBLE) {
            /* Use an ordinary double dot product after the exact neutral
             * shortcut. Centering every color on red can discard a much
             * smaller channel and introduce extra cancellation. Neither
             * formula guarantees correct rounding for arbitrary colors.
             */
            for (r = 0; r < 3; ++r) {
                result[r] = (float)(m->m[3 * r] * (double)x[0] +
                    m->m[3 * r + 1] * (double)x[1] +
                    m->m[3 * r + 2] * (double)x[2]);
            }
            ++counts.double_pixels;
        } else {
            for (c = 0; c < 3; ++c) {
                units[c] = png_chrm_float_units(encoded[c]);
            }
            for (r = 0; r < 3; ++r) {
                total = png_chrm_integer(0);
                for (c = 0; c < 3; ++c) {
                    total = png_chrm_plus(
                        total, png_chrm_times(m->n[3 * r + c], units[c]));
                }
                rounded = png_chrm_round_exact(total, m->d);
                memcpy(result + r, &rounded, sizeof(rounded));
            }
            ++counts.integer_pixels;
        }
        memcpy(output + 3 * i, result, sizeof(result));
    }
    if (stats != NULL) {
        *stats = counts;
    }
    return SIXEL_PNG_CHRM_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
