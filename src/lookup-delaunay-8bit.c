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
 * Exact Delaunay graph lookup for 8-bit palettes.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include "lookup-delaunay-8bit.h"
#include "lookup-delaunay-graph.h"
#include "sixel_atomic.h"

#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
#include <arm_neon.h>
#define SIXEL_DL8_SIMD 1
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
#include <emmintrin.h>
#define SIXEL_DL8_SIMD 1
#elif defined(__wasm_simd128__)
#include <wasm_simd128.h>
#define SIXEL_DL8_SIMD 1
#else
#define SIXEL_DL8_SIMD 0
#endif

/* Four neighbors in structure-of-arrays form. Repeating the last neighbor
 * pads a tail without introducing a new candidate or an out-of-bounds load. */
typedef struct sixel_delaunay_byte_block {
    int32_t x[4], y[4], z[4], id[4], distance[4];
} sixel_delaunay_byte_block_t;

struct sixel_lookup_delaunay_8bit {
    sixel_allocator_t *allocator;
    int ncolors, count;
    int *pal2u, *uindex, *offsets;
    int32_t *points;
    sixel_delaunay_byte_block_t *blocks;
    int seeds[512];
    sixel_atomic_i32_t *memo;
};

#define SIXEL_DELAUNAY_BYTE_MEMO_BITS 18

static void
sixel_lookup_delaunay_8bit_release(sixel_lookup_delaunay_8bit_t *graph)
{
    sixel_allocator_free(graph->allocator, graph->pal2u);
    sixel_allocator_free(graph->allocator, graph->uindex);
    sixel_allocator_free(graph->allocator, graph->offsets);
    sixel_allocator_free(graph->allocator, graph->points);
    sixel_allocator_free(graph->allocator, graph->blocks);
    sixel_allocator_free(graph->allocator, graph->memo);
    graph->pal2u = NULL;
    graph->uindex = NULL;
    graph->offsets = NULL;
    graph->points = NULL;
    graph->blocks = NULL;
    graph->memo = NULL;
    graph->count = graph->ncolors = 0;
}

SIXELSTATUS
sixel_lookup_delaunay_8bit_create(sixel_allocator_t *allocator,
                                 sixel_lookup_delaunay_8bit_t **graph_out)
{
    sixel_lookup_delaunay_8bit_t *graph;

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
sixel_lookup_delaunay_8bit_unref(sixel_lookup_delaunay_8bit_t *graph)
{
    sixel_allocator_t *allocator;

    if (graph == NULL) {
        return;
    }
    allocator = graph->allocator;
    sixel_lookup_delaunay_8bit_release(graph);
    sixel_allocator_free(allocator, graph);
    sixel_allocator_unref(allocator);
}

static int
sixel_delaunay_byte_distance(int32_t const *point,
                           unsigned char const *pixel)
{
    int x, y, z;

    x = (int)pixel[0] - point[0];
    y = (int)pixel[1] - point[1];
    z = (int)pixel[2] - point[2];
    return x * x + y * y + z * z;
}

SIXELSTATUS
sixel_lookup_delaunay_8bit_configure(sixel_lookup_delaunay_8bit_t *graph,
                                    unsigned char const *palette,
                                    int ncolors, int depth)
{
    double *wide;
    double const low[3] = {0.0, 0.0, 0.0};
    double const high[3] = {255.0, 255.0, 255.0};
    int *offsets, *neighbors;
    int i, j, c, u, pos, end, lane, blocks, out, best;
    double q[3], d, best_distance, diff;
    int32_t dx, dy, dz;
    unsigned char const *p;
    sixel_delaunay_byte_block_t *block;
    SIXELSTATUS status;

    if (graph == NULL || palette == NULL || depth < 3
            || ncolors < 1 || ncolors > 32768) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_lookup_delaunay_8bit_release(graph);
    wide = NULL;
    offsets = neighbors = NULL;
    status = SIXEL_BAD_ALLOCATION;
    graph->pal2u = sixel_allocator_malloc(graph->allocator,
                                        (size_t)ncolors * sizeof(int));
    graph->uindex = sixel_allocator_malloc(graph->allocator,
                                         (size_t)ncolors * sizeof(int));
    graph->points = sixel_allocator_malloc(graph->allocator,
                                (size_t)ncolors * 3U * sizeof(int32_t));
    wide = sixel_allocator_malloc(graph->allocator,
                                 (size_t)ncolors * 3U * sizeof(double));
    if (!graph->pal2u || !graph->uindex || !graph->points || !wide) {
        goto end;
    }
    graph->ncolors = ncolors;
    /* First occurrence order makes representative IDs and palette indices
     * agree at ties. Even a stale hint naming a duplicate reaches its owner. */
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
                wide[u * 3 + c] = p[c];
            }
            ++graph->count;
        }
    }
    status = sixel_lookup_delaunay_graph_build(graph->allocator, wide,
        graph->count, low, high, &offsets, &neighbors);
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
                dx = block->x[lane] - graph->points[u * 3];
                dy = block->y[lane] - graph->points[u * 3 + 1];
                dz = block->z[lane] - graph->points[u * 3 + 2];
                block->distance[lane] = dx * dx + dy * dy + dz * dz;
            }
        }
    }
    graph->offsets[graph->count] = out;
    for (i = 0; i < 512; ++i) {
        q[0] = ((i >> 6) + 0.5) * (255.0 / 8.0);
        q[1] = (((i >> 3) & 7) + 0.5) * (255.0 / 8.0);
        q[2] = ((i & 7) + 0.5) * (255.0 / 8.0);
        best = 0;
        best_distance = 1.0e30;
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
    if (ncolors <= 256) {
        graph->memo = sixel_allocator_malloc(graph->allocator,
            ((size_t)1 << SIXEL_DELAUNAY_BYTE_MEMO_BITS)
                * sizeof(*graph->memo));
        if (!graph->memo) {
            goto end;
        }
        for (i = 0; i < (1 << SIXEL_DELAUNAY_BYTE_MEMO_BITS); ++i) {
            sixel_atomic_store_relaxed_i32(&graph->memo[i], -1);
        }
    }
    status = SIXEL_OK;
end:
    sixel_allocator_free(graph->allocator, wide);
    sixel_allocator_free(graph->allocator, offsets);
    sixel_allocator_free(graph->allocator, neighbors);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_delaunay_8bit_release(graph);
    }
    return status;
}

/* If c is not nearest, the segment from c toward q leaves c's closed cell
 * through a bisector shared with some neighbor n. The squared-distance
 * difference |q-n|^2 - |q-c|^2 is affine along that segment: zero at the
 * crossing and negative beyond it. Thus a strictly closer neighbor exists.
 * At a minimum every tied cell contains q, so closed adjacency retains all
 * ties, including cospherical degeneracy. Lexicographic (distance, ID)
 * descent terminates and selects the smallest original palette index.
 * The triangle inequality restricts contenders to |n-c|^2 <= 4|q-c|^2.
 * Testing extra lanes in the final block cannot change this conclusion. */
static int
sixel_lookup_delaunay_8bit_walk_scalar(
    sixel_lookup_delaunay_8bit_t const *graph,
    unsigned char const *pixel, int center)
{
    int distance, best_distance, best, pos, lane, x, y, z, d;
    sixel_delaunay_byte_block_t const *block;

    distance = sixel_delaunay_byte_distance(graph->points + center * 3,
                                          pixel);
    for (;;) {
        best = center;
        best_distance = distance;
        for (pos = graph->offsets[center];
                pos < graph->offsets[center + 1]; ++pos) {
            block = &graph->blocks[pos];
            if (block->distance[0] > 4 * distance) {
                break;
            }
            for (lane = 0; lane < 4; ++lane) {
                x = (int)pixel[0] - block->x[lane];
                y = (int)pixel[1] - block->y[lane];
                z = (int)pixel[2] - block->z[lane];
                d = x * x + y * y + z * z;
                if (d < best_distance
                        || (d == best_distance && block->id[lane] < best)) {
                    best_distance = d;
                    best = block->id[lane];
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

#if SIXEL_DL8_SIMD
/* Signed packed keys fit: 195075 * 256 + 255 < INT_MAX. Larger
 * representative sets retain separate distance/ID lanes and tie masks. */
static int
sixel_lookup_delaunay_8bit_walk_simd(
    sixel_lookup_delaunay_8bit_t const *graph,
    unsigned char const *pixel, int center)
{
    int distance, best_distance, best, pos, lane, packed;
    int32_t distances[4], ids[4];
    sixel_delaunay_byte_block_t const *block;
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
    int32x4_t x, y, z, d, id, vd, vi, key;
    uint32x4_t mask;
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
    __m128i x, y, z, d, id, vd, vi, key;
    __m128i mask, xy, zz, zero;
#else
    v128_t x, y, z, d, id, vd, vi, key;
    v128_t mask;
#endif

    packed = graph->count <= 256;
    distance = sixel_delaunay_byte_distance(graph->points + center * 3,
                                          pixel);
    for (;;) {
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
        vd = vdupq_n_s32(distance);
        vi = vdupq_n_s32(center);
        key = vdupq_n_s32(packed ? (distance << 8) | center : 0);
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
        vd = _mm_set1_epi32(distance);
        vi = _mm_set1_epi32(center);
        key = _mm_set1_epi32(packed ? (distance << 8) | center : 0);
        zero = _mm_setzero_si128();
#else
        vd = wasm_i32x4_splat(distance);
        vi = wasm_i32x4_splat(center);
        key = wasm_i32x4_splat(packed ? (distance << 8) | center : 0);
#endif
        for (pos = graph->offsets[center];
                pos < graph->offsets[center + 1]; ++pos) {
            block = &graph->blocks[pos];
            if (block->distance[0] > 4 * distance) {
                break;
            }
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
            x = vsubq_s32(vdupq_n_s32(pixel[0]), vld1q_s32(block->x));
            y = vsubq_s32(vdupq_n_s32(pixel[1]), vld1q_s32(block->y));
            z = vsubq_s32(vdupq_n_s32(pixel[2]), vld1q_s32(block->z));
            d = vmulq_s32(x, x);
            d = vmlaq_s32(d, y, y);
            d = vmlaq_s32(d, z, z);
            id = vld1q_s32(block->id);
            if (packed) {
                key = vminq_s32(key, vorrq_s32(vshlq_n_s32(d, 8), id));
            } else {
                mask = vorrq_u32(vcltq_s32(d, vd),
                    vandq_u32(vceqq_s32(d, vd), vcltq_s32(id, vi)));
                vd = vbslq_s32(mask, d, vd);
                vi = vbslq_s32(mask, id, vi);
            }
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
            x = _mm_sub_epi32(_mm_set1_epi32(pixel[0]),
                _mm_loadu_si128((__m128i const *)(void const *)block->x));
            y = _mm_sub_epi32(_mm_set1_epi32(pixel[1]),
                _mm_loadu_si128((__m128i const *)(void const *)block->y));
            z = _mm_sub_epi32(_mm_set1_epi32(pixel[2]),
                _mm_loadu_si128((__m128i const *)(void const *)block->z));
            /* Each difference is in [-255,255]. Pack (dx,dy) and
             * (dz,0); madd supplies four exact 32-bit squared sums. */
            x = _mm_packs_epi32(x, zero);
            y = _mm_packs_epi32(y, zero);
            z = _mm_packs_epi32(z, zero);
            xy = _mm_unpacklo_epi16(x, y);
            zz = _mm_unpacklo_epi16(z, zero);
            d = _mm_add_epi32(_mm_madd_epi16(xy, xy),
                              _mm_madd_epi16(zz, zz));
            id = _mm_loadu_si128(
                (__m128i const *)(void const *)block->id);
            if (packed) {
                d = _mm_or_si128(_mm_slli_epi32(d, 8), id);
                mask = _mm_cmplt_epi32(d, key);
                key = _mm_or_si128(_mm_and_si128(mask, d),
                                   _mm_andnot_si128(mask, key));
            } else {
                mask = _mm_or_si128(_mm_cmplt_epi32(d, vd),
                    _mm_and_si128(_mm_cmpeq_epi32(d, vd),
                                   _mm_cmplt_epi32(id, vi)));
                vd = _mm_or_si128(_mm_and_si128(mask, d),
                                  _mm_andnot_si128(mask, vd));
                vi = _mm_or_si128(_mm_and_si128(mask, id),
                                  _mm_andnot_si128(mask, vi));
            }
#else
            x = wasm_i32x4_sub(wasm_i32x4_splat(pixel[0]),
                                  wasm_v128_load(block->x));
            y = wasm_i32x4_sub(wasm_i32x4_splat(pixel[1]),
                                  wasm_v128_load(block->y));
            z = wasm_i32x4_sub(wasm_i32x4_splat(pixel[2]),
                                  wasm_v128_load(block->z));
            d = wasm_i32x4_add(wasm_i32x4_mul(x, x),
                wasm_i32x4_add(wasm_i32x4_mul(y, y),
                               wasm_i32x4_mul(z, z)));
            id = wasm_v128_load(block->id);
            if (packed) {
                key = wasm_i32x4_min(key,
                    wasm_v128_or(wasm_i32x4_shl(d, 8), id));
            } else {
                mask = wasm_v128_or(wasm_i32x4_lt(d, vd),
                    wasm_v128_and(wasm_i32x4_eq(d, vd),
                                   wasm_i32x4_lt(id, vi)));
                vd = wasm_v128_bitselect(d, vd, mask);
                vi = wasm_v128_bitselect(id, vi, mask);
            }
#endif
        }
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
        if (packed) {
            best = vminvq_s32(key);
            best_distance = best >> 8;
            best &= 255;
        } else {
            vst1q_s32(distances, vd);
            vst1q_s32(ids, vi);
        }
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
        if (packed) {
            d = _mm_shuffle_epi32(key, _MM_SHUFFLE(1, 0, 3, 2));
            mask = _mm_cmplt_epi32(d, key);
            key = _mm_or_si128(_mm_and_si128(mask, d),
                               _mm_andnot_si128(mask, key));
            d = _mm_shuffle_epi32(key, _MM_SHUFFLE(2, 3, 0, 1));
            mask = _mm_cmplt_epi32(d, key);
            key = _mm_or_si128(_mm_and_si128(mask, d),
                               _mm_andnot_si128(mask, key));
            best = _mm_cvtsi128_si32(key);
            best_distance = best >> 8;
            best &= 255;
        } else {
            _mm_storeu_si128((__m128i *)(void *)distances, vd);
            _mm_storeu_si128((__m128i *)(void *)ids, vi);
        }
#else
        if (packed) {
            key = wasm_i32x4_min(key,
                wasm_i32x4_shuffle(key, key, 2, 3, 0, 1));
            key = wasm_i32x4_min(key,
                wasm_i32x4_shuffle(key, key, 1, 0, 3, 2));
            best = wasm_i32x4_extract_lane(key, 0);
            best_distance = best >> 8;
            best &= 255;
        } else {
            wasm_v128_store(distances, vd);
            wasm_v128_store(ids, vi);
        }
#endif
        if (!packed) {
            best = center;
            best_distance = distance;
            for (lane = 0; lane < 4; ++lane) {
                if (distances[lane] < best_distance
                        || (distances[lane] == best_distance
                            && ids[lane] < best)) {
                    best = ids[lane];
                    best_distance = distances[lane];
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
#endif

int
sixel_lookup_delaunay_8bit_map(sixel_lookup_delaunay_8bit_t const *graph,
                              unsigned char const *pixel, int hint)
{
    unsigned int key, slot, entry;
    int center, result;

    if (graph == NULL || pixel == NULL || graph->count == 0) {
        return 0;
    }
    key = ((unsigned int)pixel[0] << 16)
        | ((unsigned int)pixel[1] << 8) | pixel[2];
    slot = (key * 2654435761U) >> (32 - SIXEL_DELAUNAY_BYTE_MEMO_BITS);
    /* A complete RGB key and index share one relaxed atomic word. The
     * all-ones sentinel (white -> index 255) must never be published. */
    if (graph->memo != NULL) {
        entry = (unsigned int)sixel_atomic_load_relaxed_i32(&graph->memo[slot]);
        if (entry != (unsigned int)-1 && (entry >> 8) == key) {
            return (int)(entry & 255U);
        }
    }
    center = hint >= 0 && hint < graph->ncolors ? graph->pal2u[hint]
        : graph->seeds[((pixel[0] >> 5) << 6)
                       | ((pixel[1] >> 5) << 3) | (pixel[2] >> 5)];
#if SIXEL_DL8_SIMD
    if (graph->count > 1) {
        result = sixel_lookup_delaunay_8bit_walk_simd(graph, pixel, center);
    } else
#endif
    {
        result = sixel_lookup_delaunay_8bit_walk_scalar(graph, pixel, center);
    }
    if (graph->memo != NULL) {
        entry = (key << 8) | (unsigned int)result;
        if (entry != (unsigned int)-1) {
            sixel_atomic_store_relaxed_i32(&graph->memo[slot], (int)entry);
        }
    }
    return result;
}

void
sixel_lookup_delaunay_8bit_map_pixels(
    sixel_lookup_delaunay_8bit_t const *graph,
    unsigned char const *pixels, size_t count, int pixel_stride,
    sixel_index_t *result, int result_stride)
{
    size_t i;
    unsigned int key, previous;
    int hint;

    previous = 0x1000000U;
    hint = -1;
    for (i = 0; i < count; ++i) {
        key = ((unsigned int)pixels[0] << 16)
            | ((unsigned int)pixels[1] << 8) | pixels[2];
        if (key != previous) {
            hint = sixel_lookup_delaunay_8bit_map(graph, pixels, hint);
            previous = key;
        }
        *result = (sixel_index_t)hint;
        /* Do not form a pointer before the buffer after a reversed row. */
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
