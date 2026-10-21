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

/*
 * Exact Delaunay graph lookup for native float palettes.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include "lookup-delaunay-float32.h"
#include "lookup-delaunay-graph.h"

/* Follow the existing isolated float-lookup build policy. Unknown native
 * contraction settings and amalgamation retain the scalar expression. */
#if defined(SIXEL_FLOAT_LOOKUP_EXACT_SIMD) && !defined(__FAST_MATH__) \
    && FLT_EVAL_METHOD == 0
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
#include <arm_neon.h>
#define SIXEL_DLF_SIMD 1
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
#include <emmintrin.h>
#define SIXEL_DLF_SIMD 1
#elif defined(__wasm_simd128__)
#include <wasm_simd128.h>
#define SIXEL_DLF_SIMD 1
#endif
#endif
#ifndef SIXEL_DLF_SIMD
#define SIXEL_DLF_SIMD 0
#endif

typedef struct sixel_delaunay_float_block {
    double x[4], y[4], z[4], id[4];
    float distance[4];
} sixel_delaunay_float_block_t;

struct sixel_lookup_delaunay_float32 {
    sixel_allocator_t *allocator;
    int ncolors, count, bounded;
    int *pal2u, *uindex, *offsets;
    double *points;
    double weights_wide[3], scale[3], low[3], high[3];
    sixel_delaunay_float_block_t *blocks;
    int seeds[512];
};
static void
sixel_lookup_delaunay_float32_release(sixel_lookup_delaunay_float32_t *graph)
{
    sixel_allocator_free(graph->allocator, graph->pal2u);
    sixel_allocator_free(graph->allocator, graph->uindex);
    sixel_allocator_free(graph->allocator, graph->offsets);
    sixel_allocator_free(graph->allocator, graph->points);
    sixel_allocator_free(graph->allocator, graph->blocks);
    graph->pal2u = NULL;
    graph->uindex = NULL;
    graph->offsets = NULL;
    graph->points = NULL;
    graph->blocks = NULL;
    graph->count = graph->ncolors = graph->bounded = 0;
}

SIXELSTATUS
sixel_lookup_delaunay_float32_create(sixel_allocator_t *allocator,
                                 sixel_lookup_delaunay_float32_t **graph_out)
{
    sixel_lookup_delaunay_float32_t *graph;

    if (allocator == NULL || graph_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *graph_out = NULL;
    graph = sixel_allocator_calloc(allocator, 1, sizeof(*graph));
    if (graph == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    graph->allocator = allocator;
    sixel_allocator_ref(allocator);
    *graph_out = graph;
    return SIXEL_OK;
}

void
sixel_lookup_delaunay_float32_unref(sixel_lookup_delaunay_float32_t *graph)
{
    sixel_allocator_t *allocator;

    if (graph == NULL) {
        return;
    }
    allocator = graph->allocator;
    sixel_lookup_delaunay_float32_release(graph);
    sixel_allocator_free(allocator, graph);
    sixel_allocator_unref(allocator);
}


/* Widen each stored float exactly; preserve VP-tree's channel order and
 * term grouping. Do not compute distances in the scaled graph coordinates. */
static double
sixel_delaunay_float_distance(sixel_lookup_delaunay_float32_t const *graph,
                             float const *pixel, int id)
{
    int c;
    double diff, distance;
    double const *entry;

    entry = graph->points + id * 3;
    distance = 0.0;
    for (c = 0; c < 3; ++c) {
        diff = (double)pixel[c] - entry[c];
        distance += diff * diff * graph->weights_wide[c];
    }
    return distance;
}

/* NaNs never win a strict comparison, and an all-nonfinite scan returns
 * index zero, just as a palette-order exhaustive scan initialized to
 * DBL_MAX. */
static int
sixel_delaunay_float_scan(sixel_lookup_delaunay_float32_t const *graph,
                         float const *pixel)
{
    int i, best;
    double distance, minimum;

    best = 0;
    minimum = DBL_MAX;
    for (i = 0; i < graph->count; ++i) {
        distance = sixel_delaunay_float_distance(graph, pixel, i);
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    return graph->uindex[best];
}

SIXELSTATUS
sixel_lookup_delaunay_float32_configure(
    sixel_lookup_delaunay_float32_t *graph, float const *palette,
    int ncolors, int depth, float const *weights)
{
    double *wide;
    int *offsets, *neighbors;
    int i, j, c, u, pos, end, lane, blocks, out, best;
    double q[3], d, best_distance, diff;
    float rounded;
    float const *p;
    sixel_delaunay_float_block_t *block;
    SIXELSTATUS status;

    if (graph == NULL || palette == NULL || weights == NULL || depth < 3
            || ncolors < 1 || ncolors > 32768) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_lookup_delaunay_float32_release(graph);
    wide = NULL;
    offsets = neighbors = NULL;
    status = SIXEL_BAD_ALLOCATION;
    graph->pal2u = sixel_allocator_malloc(graph->allocator,
                                        (size_t)ncolors * sizeof(int));
    graph->uindex = sixel_allocator_malloc(graph->allocator,
                                         (size_t)ncolors * sizeof(int));
    graph->points = sixel_allocator_malloc(graph->allocator,
                                 (size_t)ncolors * 3U * sizeof(double));
    wide = sixel_allocator_malloc(graph->allocator,
                                 (size_t)ncolors * 3U * sizeof(double));
    if (!graph->pal2u || !graph->uindex || !graph->points || !wide) {
        goto end;
    }
    graph->ncolors = ncolors;
    graph->bounded = 1;
    for (c = 0; c < 3; ++c) {
        graph->weights_wide[c] = weights[c];
        if (!isfinite(weights[c]) || weights[c] <= 0.0f) {
            graph->bounded = 0;
        }
        graph->scale[c] = sqrt((double)weights[c]);
        graph->low[c] = DBL_MAX;
        graph->high[c] = -DBL_MAX;
    }
    for (i = 0; i < ncolors; ++i) {
        p = palette + (size_t)i * (size_t)depth;
        for (u = 0; u < graph->count; ++u) {
            if (graph->points[u * 3] == p[0]
                    && graph->points[u * 3 + 1] == p[1]
                    && graph->points[u * 3 + 2] == p[2]) {
                break;
            }
        }
        graph->pal2u[i] = u;
        if (u == graph->count) {
            graph->uindex[u] = i;
            for (c = 0; c < 3; ++c) {
                graph->points[u * 3 + c] = p[c];
                d = (double)p[c] * graph->scale[c];
                wide[u * 3 + c] = d;
                if (!isfinite(d)) {
                    graph->bounded = 0;
                }
                if (d < graph->low[c]) {
                    graph->low[c] = d;
                }
                if (d > graph->high[c]) {
                    graph->high[c] = d;
                }
            }
            ++graph->count;
        }
    }
    /* Nonfinite palettes or nonpositive metrics have no Euclidean graph.
     * Keep their exhaustive semantics without feeding NaNs to geometry. */
    if (!graph->bounded) {
        status = SIXEL_OK;
        goto end;
    }
    for (c = 0; c < 3; ++c) {
        graph->low[c] -= 1.0;
        graph->high[c] += 1.0;
        /* At extreme magnitudes +/-1 may round away. A nextafter expansion
         * is still an outward box and avoids a zero-width seed dimension. */
        if (graph->low[c] == graph->high[c]) {
            graph->low[c] = nextafter(graph->low[c], -INFINITY);
            graph->high[c] = nextafter(graph->high[c], INFINITY);
        }
    }
    status = sixel_lookup_delaunay_graph_build(graph->allocator, wide,
        graph->count, graph->low, graph->high, &offsets, &neighbors);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = SIXEL_BAD_ALLOCATION;
    blocks = 0;
    for (u = 0; u < graph->count; ++u) {
        blocks += (offsets[u + 1] - offsets[u] + 3) / 4;
    }
    /* Dense graphs can exceed size_t even when their edge count fits int. */
    if ((size_t)blocks >= SIZE_MAX / sizeof(*block)) {
        goto end;
    }
    graph->offsets = sixel_allocator_malloc(graph->allocator,
                              ((size_t)graph->count + 1U) * sizeof(int));
    graph->blocks = sixel_allocator_malloc(graph->allocator,
                              ((size_t)blocks + 1U) * sizeof(*block));
    if (!graph->offsets || !graph->blocks) {
        goto end;
    }
    out = 0;
    for (u = 0; u < graph->count; ++u) {
        graph->offsets[u] = out;
        end = offsets[u + 1];
        for (pos = offsets[u]; pos < end; pos += 4) {
            block = &graph->blocks[out++];
            for (lane = 0; lane < 4; ++lane) {
                j = neighbors[pos + lane < end ? pos + lane : end - 1];
                block->x[lane] = graph->points[j * 3];
                block->y[lane] = graph->points[j * 3 + 1];
                block->z[lane] = graph->points[j * 3 + 2];
                block->id[lane] = j;
                d = 0.0;
                for (c = 0; c < 3; ++c) {
                    diff = wide[u * 3 + c] - wide[j * 3 + c];
                    d += diff * diff;
                }
                /* Match the builder's monotone sort key, not a separately
                 * rounded metric that could reorder nearly equal edges. */
                rounded = (float)d;
                if ((double)rounded > d) {
                    rounded = nextafterf(rounded, 0.0f);
                }
                block->distance[lane] = rounded;
            }
        }
    }
    graph->offsets[graph->count] = out;
    for (i = 0; i < 512; ++i) {
        for (c = 0; c < 3; ++c) {
            q[c] = graph->low[c] + (((i >> (6 - c * 3)) & 7) + 0.5)
                * ((graph->high[c] - graph->low[c]) / 8.0);
        }
        best = 0;
        best_distance = DBL_MAX;
        for (u = 0; u < graph->count; ++u) {
            d = 0.0;
            for (c = 0; c < 3; ++c) {
                diff = q[c] - wide[u * 3 + c];
                d += diff * diff;
            }
            if (d < best_distance) {
                best_distance = d;
                best = u;
            }
        }
        graph->seeds[i] = best;
    }
    status = SIXEL_OK;
end:
    sixel_allocator_free(graph->allocator, wide);
    sixel_allocator_free(graph->allocator, offsets);
    sixel_allocator_free(graph->allocator, neighbors);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_delaunay_float32_release(graph);
    }
    return status;
}

/* The same closed-cell segment argument as the byte walk guarantees an
 * improving neighbor. Each move strictly decreases (distance, ID); all
 * ties are closed neighbors. Slack in the edge bound only adds evaluations. */
static int
sixel_lookup_delaunay_float32_walk_scalar(
    sixel_lookup_delaunay_float32_t const *graph,
    float const *pixel, int center)
{
    int best, pos, lane, id;
    double distance, best_distance, bound, d;
    sixel_delaunay_float_block_t const *block;

    distance = sixel_delaunay_float_distance(graph, pixel, center);
    for (;;) {
        best = center;
        best_distance = distance;
        bound = 4.0 * distance * (1.0 + 1.0e-9);
        for (pos = graph->offsets[center];
                pos < graph->offsets[center + 1]; ++pos) {
            block = &graph->blocks[pos];
            if ((double)block->distance[0] > bound) {
                break;
            }
            for (lane = 0; lane < 4; ++lane) {
                id = (int)block->id[lane];
                d = sixel_delaunay_float_distance(graph, pixel, id);
                if (d < best_distance || (d == best_distance && id < best)) {
                    best_distance = d;
                    best = id;
                }
            }
        }
        if (best == center) {
            return graph->uindex[center];
        }
        center = best;
        distance = best_distance;
    }
}

#if SIXEL_DLF_SIMD
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
typedef float64x2_t sixel_delaunay_distance2_t;
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
typedef __m128d sixel_delaunay_distance2_t;
#else
typedef v128_t sixel_delaunay_distance2_t;
#endif

/* Each lane evaluates a neighbor. Separate double products and sums follow
 * the scalar order exactly; the build gate forbids contraction. */
static sixel_delaunay_distance2_t
sixel_delaunay_float_distance2(sixel_lookup_delaunay_float32_t const *graph,
    float const *pixel, sixel_delaunay_float_block_t const *block, int half)
{
    sixel_delaunay_distance2_t diff, sum;
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
    sum = vdupq_n_f64(0.0);
    diff = vsubq_f64(vdupq_n_f64(pixel[0]), vld1q_f64(block->x + half));
    sum = vaddq_f64(sum, vmulq_n_f64(vmulq_f64(diff, diff),
                                    graph->weights_wide[0]));
    diff = vsubq_f64(vdupq_n_f64(pixel[1]), vld1q_f64(block->y + half));
    sum = vaddq_f64(sum, vmulq_n_f64(vmulq_f64(diff, diff),
                                    graph->weights_wide[1]));
    diff = vsubq_f64(vdupq_n_f64(pixel[2]), vld1q_f64(block->z + half));
    sum = vaddq_f64(sum, vmulq_n_f64(vmulq_f64(diff, diff),
                                    graph->weights_wide[2]));
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
    sum = _mm_setzero_pd();
    diff = _mm_sub_pd(_mm_set1_pd(pixel[0]), _mm_loadu_pd(block->x + half));
    sum = _mm_add_pd(sum, _mm_mul_pd(_mm_mul_pd(diff, diff),
                                    _mm_set1_pd(graph->weights_wide[0])));
    diff = _mm_sub_pd(_mm_set1_pd(pixel[1]), _mm_loadu_pd(block->y + half));
    sum = _mm_add_pd(sum, _mm_mul_pd(_mm_mul_pd(diff, diff),
                                    _mm_set1_pd(graph->weights_wide[1])));
    diff = _mm_sub_pd(_mm_set1_pd(pixel[2]), _mm_loadu_pd(block->z + half));
    sum = _mm_add_pd(sum, _mm_mul_pd(_mm_mul_pd(diff, diff),
                                    _mm_set1_pd(graph->weights_wide[2])));
#else
    sum = wasm_f64x2_splat(0.0);
    diff = wasm_f64x2_sub(wasm_f64x2_splat(pixel[0]),
                          wasm_v128_load(block->x + half));
    sum = wasm_f64x2_add(sum, wasm_f64x2_mul(wasm_f64x2_mul(diff, diff),
                                wasm_f64x2_splat(graph->weights_wide[0])));
    diff = wasm_f64x2_sub(wasm_f64x2_splat(pixel[1]),
                          wasm_v128_load(block->y + half));
    sum = wasm_f64x2_add(sum, wasm_f64x2_mul(wasm_f64x2_mul(diff, diff),
                                wasm_f64x2_splat(graph->weights_wide[1])));
    diff = wasm_f64x2_sub(wasm_f64x2_splat(pixel[2]),
                          wasm_v128_load(block->z + half));
    sum = wasm_f64x2_add(sum, wasm_f64x2_mul(wasm_f64x2_mul(diff, diff),
                                wasm_f64x2_splat(graph->weights_wide[2])));
#endif
    return sum;
}

static int
sixel_lookup_delaunay_float32_walk_simd(
    sixel_lookup_delaunay_float32_t const *graph,
    float const *pixel, int center)
{
    int best, pos, half, lane;
    double distance, best_distance, bound, distances[2], ids[2];
    sixel_delaunay_float_block_t const *block;
    sixel_delaunay_distance2_t vd, vi, d, id;
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
    uint64x2_t mask;
#else
    sixel_delaunay_distance2_t mask;
#endif

    distance = sixel_delaunay_float_distance(graph, pixel, center);
    for (;;) {
        bound = 4.0 * distance * (1.0 + 1.0e-9);
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
        vd = vdupq_n_f64(distance);
        vi = vdupq_n_f64((double)center);
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
        vd = _mm_set1_pd(distance);
        vi = _mm_set1_pd((double)center);
#else
        vd = wasm_f64x2_splat(distance);
        vi = wasm_f64x2_splat((double)center);
#endif
        for (pos = graph->offsets[center];
                pos < graph->offsets[center + 1]; ++pos) {
            block = &graph->blocks[pos];
            if ((double)block->distance[0] > bound) {
                break;
            }
            for (half = 0; half < 4; half += 2) {
                d = sixel_delaunay_float_distance2(graph, pixel, block, half);
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
                id = vld1q_f64(block->id + half);
                mask = vorrq_u64(vcltq_f64(d, vd),
                    vandq_u64(vceqq_f64(d, vd), vcltq_f64(id, vi)));
                vd = vbslq_f64(mask, d, vd);
                vi = vbslq_f64(mask, id, vi);
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
                id = _mm_loadu_pd(block->id + half);
                mask = _mm_or_pd(_mm_cmplt_pd(d, vd),
                    _mm_and_pd(_mm_cmpeq_pd(d, vd), _mm_cmplt_pd(id, vi)));
                vd = _mm_or_pd(_mm_and_pd(mask, d),
                                _mm_andnot_pd(mask, vd));
                vi = _mm_or_pd(_mm_and_pd(mask, id),
                                _mm_andnot_pd(mask, vi));
#else
                id = wasm_v128_load(block->id + half);
                mask = wasm_v128_or(wasm_f64x2_lt(d, vd),
                    wasm_v128_and(wasm_f64x2_eq(d, vd),
                                   wasm_f64x2_lt(id, vi)));
                vd = wasm_v128_bitselect(d, vd, mask);
                vi = wasm_v128_bitselect(id, vi, mask);
#endif
            }
        }
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
        vst1q_f64(distances, vd);
        vst1q_f64(ids, vi);
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
        _mm_storeu_pd(distances, vd);
        _mm_storeu_pd(ids, vi);
#else
        wasm_v128_store(distances, vd);
        wasm_v128_store(ids, vi);
#endif
        best = center;
        best_distance = distance;
        for (lane = 0; lane < 2; ++lane) {
            if (distances[lane] < best_distance
                    || (distances[lane] == best_distance
                        && (int)ids[lane] < best)) {
                best = (int)ids[lane];
                best_distance = distances[lane];
            }
        }
        if (best == center) {
            return graph->uindex[center];
        }
        center = best;
        distance = best_distance;
    }
}
#endif

int
sixel_lookup_delaunay_float32_map(
    sixel_lookup_delaunay_float32_t const *graph,
    float const *pixel, int hint)
{
    double scaled;
    int c, cell, axis, center;

    if (graph == NULL || pixel == NULL || graph->count == 0) {
        return 0;
    }
    if (!graph->bounded) {
        return sixel_delaunay_float_scan(graph, pixel);
    }
    cell = 0;
    for (c = 0; c < 3; ++c) {
        scaled = (double)pixel[c] * graph->scale[c];
        /* Negated comparisons include NaN without an integer conversion.
         * The graph proves adjacency only inside its construction box. */
        if (!(scaled >= graph->low[c] && scaled <= graph->high[c])) {
            return sixel_delaunay_float_scan(graph, pixel);
        }
        if (hint < 0 || hint >= graph->ncolors) {
            axis = (int)((scaled - graph->low[c])
                / (graph->high[c] - graph->low[c]) * 8.0);
            cell = (cell << 3) | (axis < 8 ? axis : 7);
        }
    }
    center = hint >= 0 && hint < graph->ncolors ? graph->pal2u[hint]
        : graph->seeds[cell];
#if SIXEL_DLF_SIMD
    if (graph->count > 1) {
        return sixel_lookup_delaunay_float32_walk_simd(graph, pixel, center);
    }
#endif
    return sixel_lookup_delaunay_float32_walk_scalar(graph, pixel, center);
}

/* The caller zeroes this table once per band. Full native keys distinguish
 * every float bit pattern; collisions replace entries, never alias colors.
 * No shared mutable state or synchronization is needed between workers. */
#define SIXEL_DELAUNAY_FLOAT_MEMO_SIZE 32768U

typedef struct sixel_delaunay_float_memo_entry {
    uint32_t key[3];
    unsigned int index_plus_one;
} sixel_delaunay_float_memo_entry_t;

typedef struct sixel_delaunay_float_workspace {
    sixel_delaunay_float_memo_entry_t memo[SIXEL_DELAUNAY_FLOAT_MEMO_SIZE];
} sixel_delaunay_float_workspace_t;

static unsigned int
sixel_delaunay_float_memo_slot(uint32_t const *key)
{
    uint32_t hash;

    hash = key[0] * 0x9e3779b1U;
    hash = (hash ^ key[1]) * 0x85ebca6bU;
    hash = (hash ^ key[2]) * 0xc2b2ae35U;
    hash ^= hash >> 16;
    return hash & (SIXEL_DELAUNAY_FLOAT_MEMO_SIZE - 1U);
}

static int
sixel_delaunay_float_memo_key(float const *pixel, uint32_t *key)
{
    memcpy(key, pixel, 3 * sizeof(uint32_t));
    return (key[0] & 0x7f800000U) != 0x7f800000U
        && (key[1] & 0x7f800000U) != 0x7f800000U
        && (key[2] & 0x7f800000U) != 0x7f800000U;
}

size_t
sixel_lookup_delaunay_float32_workspace_size(
    sixel_lookup_delaunay_float32_t const *tree)
{
    return tree != NULL && tree->ncolors <= 256
        ? sizeof(sixel_delaunay_float_workspace_t) : 0;
}


void
sixel_lookup_delaunay_float32_map_pixels(
    sixel_lookup_delaunay_float32_t const *graph,
    unsigned char const *pixels, size_t count, int pixel_stride,
    sixel_index_t *result, int result_stride, void *workspace)
{
    sixel_delaunay_float_workspace_t *scratch;
    sixel_delaunay_float_memo_entry_t *entry;
    uint32_t key[3], previous[3];
    float sample[3];
    int hint, finite, have_previous;
    size_t i;

    scratch = graph != NULL && graph->ncolors <= 256 ? workspace : NULL;
    hint = -1;
    have_previous = 0;
    for (i = 0; i < count; ++i) {
        memcpy(sample, pixels, sizeof(sample));
        finite = sixel_delaunay_float_memo_key(sample, key);
        entry = scratch != NULL && finite
            ? &scratch->memo[sixel_delaunay_float_memo_slot(key)] : NULL;
        if (!(have_previous && memcmp(previous, key, sizeof(key)) == 0)) {
            if (entry != NULL && entry->index_plus_one != 0
                    && memcmp(entry->key, key, sizeof(key)) == 0) {
                hint = (int)entry->index_plus_one - 1;
            } else {
                hint = sixel_lookup_delaunay_float32_map(graph, sample,
                                                        finite ? hint : -1);
                if (entry != NULL) {
                    memcpy(entry->key, key, sizeof(key));
                    entry->index_plus_one = (unsigned int)hint + 1;
                }
            }
            memcpy(previous, key, sizeof(key));
            have_previous = finite;
        }
        *result = (sixel_index_t)hint;
        if (i + 1 < count) {
            pixels += pixel_stride;
            result += result_stride;
        }
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
