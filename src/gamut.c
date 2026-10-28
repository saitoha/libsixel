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

/* CSS Color 4, 2026-10-09: local MINDE, EdgeSeeker and Ray Trace.
 * Reduce Oklab chroma along a constant-L, constant-hue ray. Clipping
 * is evaluated in the destination's linear device domain, BEFORE any ICC
 * inverse curve can hide out-of-gamut values. Monotone channel transfers
 * make this equivalent to clipping encoded destination RGB.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "gamut.h"
#include "colorspace.h"

typedef struct sixel_gamut_vtbl {
    int (*apply)(sixel_gamut_t const *, double const[3], double[3], int);
    void (*destroy)(sixel_gamut_t *);
} sixel_gamut_vtbl_t;

typedef struct sixel_edge_entry {
    double l, c, h, k;
} sixel_edge_entry_t;

struct sixel_gamut {
    sixel_gamut_vtbl_t const *vtbl;
    sixel_allocator_t *allocator;
    double forward[9];
    double backward[9];
    double minimum[3];
    double maximum[3];
    sixel_edge_entry_t *edge;
    int edge_count;
    int contains_srgb_cube;
};

static void
sixel_gamut_matrix(double const matrix[9], double const in[3], double out[3])
{
    int i;

    for (i = 0; i < 3; ++i) {
        out[i] = matrix[i * 3] * in[0] + matrix[i * 3 + 1] * in[1]
            + matrix[i * 3 + 2] * in[2];
    }
}

static int
sixel_gamut_contains(sixel_gamut_t const *mapper, double const device[3])
{
    int i;

    /* Float frame stores and the Oklab matrix pair can straddle a boundary.
     * This tolerance is numerical only, not an additional visible gamut. */
    for (i = 0; i < 3; ++i) {
        if (!isfinite(device[i]) ||
                device[i] < mapper->minimum[i] - 2e-7 ||
                device[i] > mapper->maximum[i] + 2e-7) {
            return 0;
        }
    }
    return 1;
}

static int
sixel_gamut_clip(sixel_gamut_t const *mapper, double const lab[3],
                  double clipped[3], double *delta)
{
    double linear[3];
    double device[3];
    double result[3];
    double difference;
    int i;

    if (!sixel_color_convert(linear, lab, SIXEL_COLORSPACE_OKLAB,
            SIXEL_COLORSPACE_LINEAR)) {
        return 0;
    }
    sixel_gamut_matrix(mapper->forward, linear, device);
    for (i = 0; i < 3; ++i) {
        if (!isfinite(device[i])) {
            return 0;
        }
        device[i] = fmax(mapper->minimum[i],
            fmin(mapper->maximum[i], device[i]));
    }
    sixel_gamut_matrix(mapper->backward, device, clipped);
    if (delta == NULL) {
        /* EdgeSeeker only needs the bounded color, not a clipping error. */
        return 1;
    }
    if (!sixel_color_convert(result, clipped, SIXEL_COLORSPACE_LINEAR,
            SIXEL_COLORSPACE_OKLAB)) {
        return 0;
    }
    *delta = 0.0;
    for (i = 0; i < 3; ++i) {
        difference = result[i] - lab[i];
        *delta = hypot(*delta, difference);
    }
    return isfinite(*delta);
}

static int
sixel_gamut_local_minde(sixel_gamut_t const *mapper, double const input[3],
                        double output[3], int colorspace)
{
    double linear[3];
    double device[3];
    double lab[3];
    double candidate[3];
    double clipped[3];
    double result[3];
    double chroma;
    double low;
    double high;
    double middle;
    double delta;
    int low_inside;
    int iteration;

    if (!sixel_color_convert(linear, input, colorspace,
            SIXEL_COLORSPACE_LINEAR)) {
        return 0;
    }
    sixel_gamut_matrix(mapper->forward, linear, device);
    if (sixel_gamut_contains(mapper, device)) {
        /* Identity must not acquire a conversion round trip. */
        memcpy(output, input, 3u * sizeof(double));
        return 1;
    }
    if (!sixel_color_convert(lab, linear, SIXEL_COLORSPACE_LINEAR,
            SIXEL_COLORSPACE_OKLAB)) {
        return 0;
    }
    if (lab[0] >= 1.0 || lab[0] <= 0.0) {
        sixel_gamut_matrix(mapper->backward,
            lab[0] >= 1.0 ? mapper->maximum : mapper->minimum, clipped);
    } else {
        if (!sixel_gamut_clip(mapper, lab, clipped, &delta)) {
            return 0;
        }
        chroma = hypot(lab[1], lab[2]);
        low = 0.0;
        high = chroma;
        low_inside = 1;
        /* Bounded even for finite, extreme extended coordinates. The
         * ordinary CSS epsilon stop needs far fewer than 128 iterations. */
        for (iteration = 0; delta >= 0.02 && high - low > 0.0001 &&
                iteration < 128; ++iteration) {
            middle = low + (high - low) * 0.5;
            candidate[0] = lab[0];
            candidate[1] = lab[1] * (middle / chroma);
            candidate[2] = lab[2] * (middle / chroma);
            if (!sixel_color_convert(linear, candidate,
                    SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_LINEAR)) {
                return 0;
            }
            sixel_gamut_matrix(mapper->forward, linear, device);
            if (low_inside && sixel_gamut_contains(mapper, device)) {
                low = middle;
                continue;
            }
            if (!sixel_gamut_clip(mapper, candidate, clipped, &delta)) {
                return 0;
            }
            if (delta < 0.02) {
                if (0.02 - delta < 0.0001) {
                    break;
                }
                low_inside = 0;
                low = middle;
                /* Keep searching up to JND, not the first smaller error. */
                delta = 0.02;
            } else {
                high = middle;
            }
        }
    }
    if (!sixel_color_convert(result, clipped, SIXEL_COLORSPACE_LINEAR,
            colorspace)) {
        return 0;
    }
    memcpy(output, result, sizeof(result));
    return 1;
}

/* EdgeSeeker samples six RGB edges in encoded device coordinates. The
 * table is immutable after construction; no worker builds or mutates it.
 * N=400 samples per region is the CSS reference's example resolution. */
#define SIXEL_EDGE_SAMPLES 400
#define SIXEL_EDGE_CAPACITY (6 * SIXEL_EDGE_SAMPLES + 2)

typedef struct sixel_edge_scratch {
    sixel_edge_entry_t cusp[SIXEL_EDGE_CAPACITY];
    sixel_edge_entry_t curve[SIXEL_EDGE_CAPACITY];
    sixel_edge_entry_t region[SIXEL_EDGE_SAMPLES];
    sixel_edge_entry_t kept[SIXEL_EDGE_SAMPLES + 1];
} sixel_edge_scratch_t;

static double
sixel_edge_step(double a, double b)
{
    double step;

    step = fmod(b - a, 360.0);
    return step < 0.0 ? step + 360.0 : step;
}

static sixel_edge_entry_t
sixel_edge_lerp(sixel_edge_entry_t a, sixel_edge_entry_t b, double h)
{
    sixel_edge_entry_t result;
    double t;
    double span;
    double offset;

    span = sixel_edge_step(a.h, b.h);
    offset = sixel_edge_step(a.h, h);
    /* A fold bridge can lie just BEFORE the lower endpoint. Unwrap to
     * its nearest hue before clamping, rather than treating it as +360. */
    if (offset > 180.0) {
        offset -= 360.0;
    }
    t = span == 0.0 ? 0.0 : offset / span;
    t = fmax(0.0, fmin(1.0, t));
    result.l = a.l + t * (b.l - a.l);
    result.c = a.c + t * (b.c - a.c);
    result.h = h;
    result.k = a.k + t * (b.k - a.k);
    return result;
}

static int
sixel_edge_compare(void const *left, void const *right)
{
    sixel_edge_entry_t const *a;
    sixel_edge_entry_t const *b;

    a = left;
    b = right;
    return a->h < b->h ? -1 : a->h > b->h;
}

static sixel_edge_entry_t
sixel_edge_lookup(sixel_edge_entry_t const *table, int count, double h)
{
    int low;
    int high;
    int mid;

    low = 0;
    high = count - 1;
    while (high - low > 1) {
        mid = low + (high - low) / 2;
        if (table[mid].h <= h) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return sixel_edge_lerp(table[low], table[high], h);
}

static int
sixel_edge_unfold(sixel_edge_scratch_t *scratch)
{
    sixel_edge_entry_t *samples;
    sixel_edge_entry_t *kept;
    sixel_edge_entry_t resume;
    sixel_edge_entry_t bridge;
    double step;
    int count;
    int i;
    int j;
    int first;

    samples = scratch->region;
    kept = scratch->kept;
    kept[0] = samples[0];
    count = 1;
    for (i = 1; i < SIXEL_EDGE_SAMPLES; i = j + 1) {
        j = i;
        if (sixel_edge_step(samples[i - 1].h, samples[i].h) <= 180.0) {
            kept[count++] = samples[i];
            continue;
        }
        while (j + 1 < SIXEL_EDGE_SAMPLES &&
                sixel_edge_step(samples[j].h, samples[j + 1].h) > 180.0) {
            ++j;
        }
        resume = samples[j];
        for (first = 0; first < count; ++first) {
            if (sixel_edge_step(kept[first].h, resume.h) > 180.0) {
                break;
            }
        }
        if (first == 0 || first == count) {
            return 0;
        }
        /* Remove the overshoot BEFORE a hue fold. Discarding only the
         * backward samples keeps the wrong edge near the blue cusp. */
        bridge = sixel_edge_lerp(kept[first - 1], kept[first],
            sixel_edge_step(0.0, resume.h - 0.0001));
        count = first;
        step = sixel_edge_step(kept[count - 1].h, bridge.h);
        if (step > 0.0 && step <= 180.0) {
            kept[count++] = bridge;
        }
        kept[count++] = resume;
    }
    return count;
}

static int
sixel_edge_samples(sixel_gamut_t const *mapper,
                     sixel_color_def_t const *definition, int transfer,
                     sixel_edge_scratch_t *scratch, int upper)
{
    static double const vertices[7][3] = {
        {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 1, 1},
        {0, 0, 1}, {1, 0, 1}, {1, 0, 0}
    };
    sixel_edge_entry_t *table;
    sixel_edge_entry_t end;
    double rgb[3];
    double device[3];
    double linear[3];
    double lab[3];
    double t;
    int region;
    int i;
    int c;
    int count;
    int kept;

    table = upper ? scratch->curve : scratch->cusp;
    count = 1;
    for (region = 0; region < 6; ++region) {
        for (i = 0; i < SIXEL_EDGE_SAMPLES; ++i) {
            t = (double)i / (SIXEL_EDGE_SAMPLES - 1);
            for (c = 0; c < 3; ++c) {
                rgb[c] = vertices[region][c] + t *
                    (vertices[region + 1][c] - vertices[region][c]);
                if (upper) {
                    rgb[c] = 0.5 + 0.5 * rgb[c];
                }
                if (transfer == SIXEL_COLORSPACE_SMPTEC) {
                    device[c] = pow(rgb[c], 2.2);
                } else {
                    device[c] = rgb[c];
                }
            }
            if (transfer != SIXEL_COLORSPACE_SMPTEC &&
                    transfer != SIXEL_COLORSPACE_LINEAR &&
                    !sixel_cdef_transfer(definition, rgb, device, 0)) {
                return 0;
            }
            sixel_gamut_matrix(mapper->backward, device, linear);
            if (!sixel_color_convert(lab, linear, SIXEL_COLORSPACE_LINEAR,
                    SIXEL_COLORSPACE_OKLAB)) {
                return 0;
            }
            scratch->region[i].l = lab[0];
            scratch->region[i].c = hypot(lab[1], lab[2]);
            scratch->region[i].h = sixel_edge_step(0.0,
                atan2(lab[2], lab[1]) * (180.0 / 3.14159265358979323846));
            scratch->region[i].k = 0.0;
        }
        kept = sixel_edge_unfold(scratch);
        if (kept < 2 || count + kept > SIXEL_EDGE_CAPACITY) {
            return 0;
        }
        memcpy(table + count, scratch->kept,
            (size_t)(kept - 1) * sizeof(*table));
        count += kept - 1;
    }
    qsort(table + 1, (size_t)(count - 1), sizeof(*table), sixel_edge_compare);
    end = sixel_edge_lerp(table[count - 1], table[1], 360.0);
    table[count++] = end;
    end.h = 0.0;
    table[0] = end;
    /* Coalesce exact duplicate hues at the seam or at a region boundary.
     * Fold bridges remain distinct and must not be pruned away. */
    kept = 1;
    for (i = 1; i < count; ++i) {
        if (table[i].h > table[kept - 1].h) {
            table[kept++] = table[i];
        }
    }
    return kept;
}

static SIXELSTATUS
sixel_edge_build(sixel_gamut_t *mapper, sixel_color_def_t const *definition,
                   int transfer, sixel_allocator_t *allocator)
{
    sixel_edge_scratch_t *scratch;
    sixel_edge_entry_t cusp;
    sixel_edge_entry_t curve;
    double x;
    double y;
    double a;
    double b;
    double k;
    int count;
    int curves;
    int i;
    int ok;

    /* This construction assumes neutral black/white at the cube ends.
     * Offset or unnormalized ICC curves require another boundary model. */
    for (i = 0; i < 3; ++i) {
        if (fabs(mapper->minimum[i]) > 2e-7 ||
                fabs(mapper->maximum[i] - 1.0) > 2e-7) {
            return SIXEL_BAD_ARGUMENT;
        }
    }
    scratch = sixel_allocator_calloc(allocator, 1u, sizeof(*scratch));
    if (scratch == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    count = sixel_edge_samples(mapper, definition, transfer, scratch, 0);
    curves = sixel_edge_samples(mapper, definition, transfer, scratch, 1);
    ok = count > 2 && curves > 2;
    for (i = 0; ok && i < count; ++i) {
        cusp = scratch->cusp[i];
        curve = sixel_edge_lookup(scratch->curve, curves, cusp.h);
        ok = cusp.l > 0.0 && cusp.l < 1.0 && cusp.c > 0.0;
        if (!ok) {
            break;
        }
        x = (1.0 - curve.l) / (1.0 - cusp.l);
        y = curve.c / cusp.c;
        /* The fitted circle has center (A/D, B/D). Cancel D before
         * taking the radius: no distant center or near-zero division.
         * Signed curvature is sign((A-B)/D) * abs(D)/hypot(A,B). */
        a = x * x + y * y - 2.0 * y;
        b = 2.0 * x - x * x - y * y;
        k = (a >= b ? 2.0 : -2.0) * (x - y) / hypot(a, b);
        ok = isfinite(k) && fabs(k) < 1.0;
        scratch->cusp[i].k = k;
    }
    if (ok) {
        mapper->edge = sixel_allocator_malloc(allocator,
            (size_t)count * sizeof(*mapper->edge));
        if (mapper->edge == NULL) {
            sixel_allocator_free(allocator, scratch);
            return SIXEL_BAD_ALLOCATION;
        }
        memcpy(mapper->edge, scratch->cusp,
            (size_t)count * sizeof(*mapper->edge));
        mapper->edge_count = count;
    }
    sixel_allocator_free(allocator, scratch);
    return ok ? SIXEL_OK : SIXEL_BAD_ARGUMENT;
}

static int
sixel_gamut_edge_seeker(sixel_gamut_t const *mapper, double const input[3],
                        double output[3], int colorspace)
{
    sixel_edge_entry_t cusp;
    double linear[3];
    double device[3];
    double lab[3];
    double clipped[3];
    double result[3];
    double chroma;
    double limit;
    double hue;
    double x;
    double t;
    double b;
    double d;

    if (!sixel_color_convert(linear, input, colorspace,
            SIXEL_COLORSPACE_LINEAR)) {
        return 0;
    }
    sixel_gamut_matrix(mapper->forward, linear, device);
    if (sixel_gamut_contains(mapper, device)) {
        memcpy(output, input, 3u * sizeof(double));
        return 1;
    }
    if (!sixel_color_convert(lab, linear, SIXEL_COLORSPACE_LINEAR,
            SIXEL_COLORSPACE_OKLAB)) {
        return 0;
    }
    if (lab[0] >= 1.0 || lab[0] <= 0.0) {
        sixel_gamut_matrix(mapper->backward,
            lab[0] >= 1.0 ? mapper->maximum : mapper->minimum, clipped);
    } else {
        chroma = hypot(lab[1], lab[2]);
        hue = sixel_edge_step(0.0, atan2(lab[2], lab[1]) *
            (180.0 / 3.14159265358979323846));
        cusp = sixel_edge_lookup(mapper->edge, mapper->edge_count, hue);
        if (lab[0] <= cusp.l) {
            limit = lab[0] / cusp.l * cusp.c;
        } else {
            x = (1.0 - lab[0]) / (1.0 - cusp.l);
            t = sqrt(2.0 - cusp.k * cusp.k);
            b = t - cusp.k;
            d = x * (t + cusp.k * (1.0 - x));
            /* Rationalized circle intersection remains stable at k=0. */
            limit = cusp.c * (2.0 * d) /
                (b + sqrt(fmax(0.0, b * b + 4.0 * cusp.k * d)));
        }
        if (chroma > limit) {
            lab[1] *= limit / chroma;
            lab[2] *= limit / chroma;
        }
        /* The fitted boundary is approximate. A final destination clip
         * is part of EdgeSeeker, not a local-MINDE search or wire snap. */
        if (!sixel_gamut_clip(mapper, lab, clipped, NULL)) {
            return 0;
        }
    }
    if (!sixel_color_convert(result, clipped, SIXEL_COLORSPACE_LINEAR,
            colorspace)) {
        return 0;
    }
    memcpy(output, result, sizeof(result));
    return 1;
}

/* Intersect a forward ray with the destination's linear RGB box. The
 * slab intervals also handle an outside anchor and parallel directions.
 * An inside anchor uses the exit, not a segment limited to t <= 1. */
static int
sixel_ray_box(sixel_gamut_t const *mapper, double const start[3],
                double const end[3], double hit[3])
{
    double tnear = -INFINITY;
    double tfar = INFINITY;
    double direction[3];
    double t1;
    double t2;
    int c;

    for (c = 0; c < 3; ++c) {
        direction[c] = end[c] - start[c];
        if (!isfinite(start[c]) || !isfinite(direction[c])) {
            return 0;
        }
        if (fabs(direction[c]) > 1e-12) {
            t1 = (mapper->minimum[c] - start[c]) / direction[c];
            t2 = (mapper->maximum[c] - start[c]) / direction[c];
            tnear = fmax(tnear, fmin(t1, t2));
            tfar = fmin(tfar, fmax(t1, t2));
        } else if (start[c] < mapper->minimum[c] ||
                start[c] > mapper->maximum[c]) {
            return 0;
        }
    }
    if (tnear > tfar || tfar < 0.0) {
        return 0;
    }
    if (tnear < 0.0) {
        tnear = tfar;
    }
    if (!isfinite(tnear)) {
        return 0;
    }
    for (c = 0; c < 3; ++c) {
        hit[c] = start[c] + direction[c] * tnear;
        if (!isfinite(hit[c])) {
            return 0;
        }
    }
    return 1;
}

/* CSS Color 4 Ray Trace, pinned to the 2026-10-09 draft. Keep the
 * original lightness/hue while successive RGB intersections approximate
 * the constant-lightness Oklab boundary. No boundary table is required. */
static int
sixel_gamut_ray_trace(sixel_gamut_t const *mapper, double const input[3],
                       double output[3], int colorspace)
{
    double linear[3];
    double device[3];
    double lab[3];
    double anchor[3];
    double hit[3];
    double last[3];
    double result[3];
    double lightness;
    double chroma;
    double hue_a;
    double hue_b;
    int iteration;
    int inside;
    int converged;
    int c;

    if (!sixel_color_convert(linear, input, colorspace,
            SIXEL_COLORSPACE_LINEAR)) {
        return 0;
    }
    sixel_gamut_matrix(mapper->forward, linear, device);
    for (c = 0; c < 3; ++c) {
        if (!isfinite(device[c])) {
            return 0;
        }
    }
    if (sixel_gamut_contains(mapper, device)) {
        memcpy(output, input, 3u * sizeof(double));
        return 1;
    }
    if (!sixel_color_convert(lab, linear, SIXEL_COLORSPACE_LINEAR,
            SIXEL_COLORSPACE_OKLAB)) {
        return 0;
    }
    lightness = lab[0];
    if (lightness >= 1.0 || lightness <= 0.0) {
        memcpy(device, lightness >= 1.0 ? mapper->maximum : mapper->minimum,
            sizeof(device));
    } else {
        chroma = hypot(lab[1], lab[2]);
        hue_a = chroma > 0.0 ? lab[1] / chroma : 0.0;
        hue_b = chroma > 0.0 ? lab[2] / chroma : 0.0;
        lab[1] = lab[2] = 0.0;
        if (!sixel_color_convert(linear, lab, SIXEL_COLORSPACE_OKLAB,
                SIXEL_COLORSPACE_LINEAR)) {
            return 0;
        }
        sixel_gamut_matrix(mapper->forward, linear, anchor);
        for (c = 0; c < 3; ++c) {
            if (!isfinite(anchor[c])) {
                return 0;
            }
        }
        memcpy(last, device, sizeof(last));
        for (iteration = 0; iteration < 4; ++iteration) {
            if (iteration > 0) {
                sixel_gamut_matrix(mapper->backward, device, linear);
                if (!sixel_color_convert(lab, linear,
                        SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_OKLAB)) {
                    return 0;
                }
                chroma = hypot(lab[1], lab[2]);
                lab[0] = lightness;
                lab[1] = hue_a * chroma;
                lab[2] = hue_b * chroma;
                if (!sixel_color_convert(linear, lab,
                        SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_LINEAR)) {
                    return 0;
                }
                sixel_gamut_matrix(mapper->forward, linear, device);
            }
            /* Arithmetic failure is not the geometric no-hit recovery. */
            for (c = 0; c < 3; ++c) {
                if (!isfinite(device[c])) {
                    return 0;
                }
            }
            /* Once the correction is within the float/matrix error budget,
             * the last hit already represents this boundary point. A tiny
             * inward correction past an advanced anchor can reverse the
             * ray; extending it to the opposite face amplifies matrix
             * round-trip error into a visible hue discontinuity. Measure
             * all channels relative to their decoded destination range. */
            converged = iteration > 0;
            for (c = 0; c < 3; ++c) {
                converged = converged && fabs(device[c] - last[c]) <=
                    2e-7 * (mapper->maximum[c] - mapper->minimum[c]);
            }
            if (converged) {
                memcpy(device, last, sizeof(device));
                break;
            }
            if (!sixel_ray_box(mapper, anchor, device, hit)) {
                /* Extreme imaginary colors can miss the box after a
                 * correction. Retain the last hit, as specified, rather
                 * than selecting another gamut mapping algorithm. */
                memcpy(device, last, sizeof(device));
                break;
            }
            inside = iteration > 0;
            for (c = 0; c < 3; ++c) {
                inside = inside && device[c] > mapper->minimum[c] + 1e-12
                    && device[c] < mapper->maximum[c] - 1e-12;
            }
            if (inside) {
                /* An inward correction advances the anchor; it must not
                 * terminate the search before the next boundary hit. */
                memcpy(anchor, device, sizeof(anchor));
            }
            memcpy(device, hit, sizeof(device));
            memcpy(last, hit, sizeof(last));
        }
    }
    for (c = 0; c < 3; ++c) {
        if (!isfinite(device[c])) {
            return 0;
        }
        device[c] = fmax(mapper->minimum[c],
            fmin(mapper->maximum[c], device[c]));
    }
    sixel_gamut_matrix(mapper->backward, device, linear);
    if (!sixel_color_convert(result, linear, SIXEL_COLORSPACE_LINEAR,
            colorspace)) {
        return 0;
    }
    memcpy(output, result, sizeof(result));
    return 1;
}

static void
sixel_gamut_destroy(sixel_gamut_t *mapper)
{
    sixel_allocator_t *allocator;

    allocator = mapper->allocator;
    sixel_allocator_free(allocator, mapper->edge);
    sixel_allocator_free(allocator, mapper);
    sixel_allocator_unref(allocator);
}

static sixel_gamut_vtbl_t const sixel_gamut_local_minde_vtbl = {
    sixel_gamut_local_minde, sixel_gamut_destroy
};

static sixel_gamut_vtbl_t const sixel_gamut_edge_seeker_vtbl = {
    sixel_gamut_edge_seeker, sixel_gamut_destroy
};

static sixel_gamut_vtbl_t const sixel_gamut_ray_trace_vtbl = {
    sixel_gamut_ray_trace, sixel_gamut_destroy
};

SIXELSTATUS
sixel_gamut_new(sixel_gamut_t **out, sixel_color_def_t const *destination,
                 int legacy_colorspace, int policy,
                 sixel_allocator_t *allocator)
{
    sixel_gamut_t *mapper;
    sixel_color_def_t *srgb;
    sixel_color_def_t *source;
    sixel_color_def_t *target;
    double basis[3];
    double xyz[3];
    double forward[3];
    double backward[3];
    double low, high, margin;
    double zero[3] = {0.0, 0.0, 0.0};
    double one[3] = {1.0, 1.0, 1.0};
    int i;
    int j;
    int ok;
    SIXELSTATUS status;

    if (out == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *out = NULL;
    if (policy != SIXEL_GAMUT_LOCAL_MINDE &&
            policy != SIXEL_GAMUT_EDGE_SEEKER &&
            policy != SIXEL_GAMUT_RAY_TRACE) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = SIXEL_OK;
    srgb = sixel_cdef_name("srgb");
    source = sixel_cdef_linearized(srgb);
    target = sixel_cdef_linearized(destination != NULL ? destination : srgb);
    mapper = sixel_allocator_calloc(allocator, 1u, sizeof(*mapper));
    ok = source != NULL && target != NULL && mapper != NULL;
    if (ok) {
        memcpy(mapper->maximum, one, sizeof(one));
        if (destination != NULL) {
            ok = sixel_cdef_transfer(destination, zero, mapper->minimum, 0)
                && sixel_cdef_transfer(destination, one, mapper->maximum, 0)
                && sixel_cdef_output_ready(destination, 1, 1);
        }
    }
    for (i = 0; ok && i < 3; ++i) {
        ok = mapper->minimum[i] < mapper->maximum[i];
        memcpy(basis, zero, sizeof(basis));
        basis[i] = 1.0;
        if (destination == NULL && legacy_colorspace ==
                SIXEL_COLORSPACE_SMPTEC) {
            ok = ok && sixel_color_convert_output(forward, basis,
                SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_SMPTEC)
                && sixel_color_from_output(backward, basis,
                    SIXEL_COLORSPACE_SMPTEC, SIXEL_COLORSPACE_LINEAR);
            for (j = 0; ok && j < 3; ++j) {
                forward[j] = copysign(pow(fabs(forward[j]), 2.2),
                    forward[j]);
            }
        } else {
            ok = ok && sixel_cdef_to_xyz(source, basis, xyz, 1)
                && sixel_cdef_from_xyz(target, xyz, forward, 1)
                && sixel_cdef_to_xyz(target, basis, xyz, 1)
                && sixel_cdef_from_xyz(source, xyz, backward, 1);
        }
        for (j = 0; ok && j < 3; ++j) {
            mapper->forward[j * 3 + i] = forward[j];
            mapper->backward[j * 3 + i] = backward[j];
        }
    }
    if (ok && policy == SIXEL_GAMUT_EDGE_SEEKER) {
        status = sixel_edge_build(mapper,
            destination != NULL ? destination : srgb,
            destination == NULL ? legacy_colorspace
                : SIXEL_COLORSPACE_GAMMA, allocator);
        ok = SIXEL_SUCCEEDED(status);
    }
    sixel_cdef_free(target);
    sixel_cdef_free(source);
    sixel_cdef_free(srgb);
    if (!ok) {
        if (mapper == NULL || status == SIXEL_BAD_ALLOCATION) {
            sixel_allocator_free(allocator, mapper);
            return SIXEL_BAD_ALLOCATION;
        }
        sixel_allocator_free(allocator, mapper);
        sixel_helper_set_additional_message(
            policy == SIXEL_GAMUT_EDGE_SEEKER
            ? "edge-seeker requires a supported matrix/TRC RGB gamut "
              "with normalized endpoints and a valid boundary table."
            : policy == SIXEL_GAMUT_RAY_TRACE
            ? "ray-trace requires a supported matrix/TRC RGB gamut."
            : "local-minde requires a supported matrix/TRC RGB gamut.");
        return SIXEL_BAD_ARGUMENT;
    }
    /* A linear map reaches each box extremum at a cube vertex. Prove
     * once that every [0,1] sRGB triplet is accepted by this destination's
     * actual matrix/ranges. The rounding margin makes this a conservative
     * certificate, not an approximate identity-matrix test. Failure only
     * disables the shortcut; extended samples still use the matrix. */
    mapper->contains_srgb_cube = 1;
    for (i = 0; i < 3; ++i) {
        low = high = 0.0;
        for (j = 0; j < 3; ++j) {
            if (!isfinite(mapper->forward[i * 3 + j])) {
                mapper->contains_srgb_cube = 0;
            }
            low += fmin(0.0, mapper->forward[i * 3 + j]);
            high += fmax(0.0, mapper->forward[i * 3 + j]);
        }
        margin = 8.0 * DBL_EPSILON * (fabs(low) + fabs(high) + 1.0);
        if (!isfinite(low) || !isfinite(high) ||
                low - margin < mapper->minimum[i] - 2e-7 ||
                high + margin > mapper->maximum[i] + 2e-7) {
            mapper->contains_srgb_cube = 0;
        }
    }
    mapper->vtbl = policy == SIXEL_GAMUT_EDGE_SEEKER
        ? &sixel_gamut_edge_seeker_vtbl : policy == SIXEL_GAMUT_RAY_TRACE
        ? &sixel_gamut_ray_trace_vtbl : &sixel_gamut_local_minde_vtbl;
    mapper->allocator = allocator;
    sixel_allocator_ref(allocator);
    *out = mapper;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_gamut_clone(sixel_gamut_t const *source, sixel_gamut_t **out)
{
    sixel_gamut_t *copy;
    size_t bytes;

    *out = NULL;
    copy = sixel_allocator_malloc(source->allocator, sizeof(*copy));
    if (copy == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    *copy = *source;
    copy->edge = NULL;
    if (source->edge != NULL) {
        bytes = (size_t)source->edge_count * sizeof(*source->edge);
        copy->edge = sixel_allocator_malloc(source->allocator, bytes);
        if (copy->edge == NULL) {
            sixel_allocator_free(source->allocator, copy);
            return SIXEL_BAD_ALLOCATION;
        }
        memcpy(copy->edge, source->edge, bytes);
    }
    sixel_allocator_ref(copy->allocator);
    *out = copy;
    return SIXEL_OK;
}

int
sixel_gamut_apply(sixel_gamut_t const *mapper, double const input[3],
                   double output[3], int colorspace)
{
    return mapper != NULL && input != NULL && output != NULL &&
        mapper->vtbl->apply(mapper, input, output, colorspace);
}

/* The scalar API is useful for individual palette entries, but calling
 * it once per image pixel repeats dispatch, generic color conversion and
 * double/float publication even when the source is already linear and the
 * destination contains it. Keep the common membership test in this loop.
 * Use the same matrix and tolerance as the scalar algorithms: do not assume
 * that [0,1] linear sRGB describes a P3 or matrix/TRC ICC destination. */
SIXELSTATUS
sixel_gamut_apply_float32(sixel_gamut_t const *mapper, float *pixels,
    size_t count, int colorspace, unsigned char const *hidden,
    size_t hidden_count)
{
    double input[3];
    double device[3];
    double output[3];
    size_t i;
    int c;

    if (mapper == NULL || (pixels == NULL && count != 0)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    for (i = 0; i < count; ++i) {
        if (hidden != NULL && i < hidden_count && hidden[i] != 0) {
            continue;
        }
        if (colorspace == SIXEL_COLORSPACE_LINEAR &&
                mapper->contains_srgb_cube &&
                pixels[i * 3u] >= 0.0f && pixels[i * 3u] <= 1.0f &&
                pixels[i * 3u + 1u] >= 0.0f &&
                pixels[i * 3u + 1u] <= 1.0f &&
                pixels[i * 3u + 2u] >= 0.0f &&
                pixels[i * 3u + 2u] <= 1.0f) {
            continue;
        }
        for (c = 0; c < 3; ++c) {
            input[c] = pixels[i * 3u + (size_t)c];
        }
        if (colorspace == SIXEL_COLORSPACE_LINEAR) {
            sixel_gamut_matrix(mapper->forward, input, device);
            if (sixel_gamut_contains(mapper, device)) {
                /* Preserve the original float bits, including signed zero.
                 * Nonfinite values cannot pass the membership predicate. */
                continue;
            }
        }
        if (!sixel_gamut_apply(mapper, input, output, colorspace)) {
            sixel_helper_set_additional_message(
                "gamut mapping cannot map this source color.");
            return SIXEL_BAD_INPUT;
        }
        for (c = 0; c < 3; ++c) {
            if (!isfinite(output[c]) || fabs(output[c]) > FLT_MAX) {
                return SIXEL_BAD_INPUT;
            }
        }
        /* Validate all components before changing the failing triplet. */
        for (c = 0; c < 3; ++c) {
            pixels[i * 3u + (size_t)c] = (float)output[c];
        }
    }
    return SIXEL_OK;
}

void
sixel_gamut_free(sixel_gamut_t *mapper)
{
    if (mapper != NULL) {
        mapper->vtbl->destroy(mapper);
    }
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
