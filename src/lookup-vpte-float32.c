/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Voronoi + 3D EDT lookup implementation for float32 pixel buffers.  The
 * structure mirrors the 8bit variant while operating on normalized palette and
 * pixel samples.  A boundary bitset triggers optional refinement using the
 * eight surrounding lattice vertices.
 */

#include "config.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <sixel.h>

#include "allocator.h"
#include "compat_stub.h"
#include "lookup-vpte-float32.h"
#include "sixel_atomic.h"
#include "status.h"

#ifndef SIXEL_VPTE_TLS
# if defined(SIXEL_ENABLE_THREADS)
#  if defined(_MSC_VER)
#   define SIXEL_VPTE_TLS __declspec(thread)
#   define SIXEL_VPTE_TLS_AVAILABLE 1
#  elif !defined(__STDC_NO_THREADS__)
#   define SIXEL_VPTE_TLS _Thread_local
#   define SIXEL_VPTE_TLS_AVAILABLE 1
#  elif defined(__GNUC__)
#   define SIXEL_VPTE_TLS __thread
#   define SIXEL_VPTE_TLS_AVAILABLE 1
#  else
#   define SIXEL_VPTE_TLS
#   define SIXEL_VPTE_TLS_AVAILABLE 0
#  endif
# else
#  define SIXEL_VPTE_TLS
#  define SIXEL_VPTE_TLS_AVAILABLE 0
# endif
#endif

struct sixel_lookup_vpte_shared {
    sixel_atomic_u32_t refcount;
    int resolution;
    int refine;
    int use_dist2;
    int weights[3];
    int ncolors;
    int depth;
    double safe_radius2;
    int use_u16;
    float *palette;
    unsigned char *palette_quant;
    float *dist2;
    uint8_t *indices8;
    uint16_t *indices16;
    unsigned char *boundary;
    uint32_t signature;
};

static int const sixel_lookup_vpte_resolution_min = 64;
static int const sixel_lookup_vpte_resolution_max = 256;

static int
sixel_lookup_vpte_pow2_log(int value)
{
    int shift;

    shift = 0;
    while ((1 << shift) < value && shift < 8) {
        ++shift;
    }

    return shift;
}

static int
sixel_lookup_vpte_validate_resolution(int resolution)
{
    int shift;
    int pow2;

    if (resolution < sixel_lookup_vpte_resolution_min
        || resolution > sixel_lookup_vpte_resolution_max) {
        return 0;
    }

    shift = sixel_lookup_vpte_pow2_log(resolution);
    pow2 = 1 << shift;

    return pow2 == resolution;
}

static uint32_t
sixel_lookup_vpte_mix_u32(uint32_t state, uint32_t value)
{
    state ^= value + 0x9e3779b9U + (state << 6) + (state >> 2);

    return state;
}

typedef struct sixel_lookup_vpte_cache_set {
    uint32_t key[4];
    int value[4];
    uint8_t hand;
} sixel_lookup_vpte_cache_set_t;

typedef struct sixel_lookup_vpte_cache {
    sixel_lookup_vpte_cache_set_t sets[16];
    uint32_t signature;
    sixel_lookup_vpte_shared_t const *shared;
} sixel_lookup_vpte_cache_t;

static SIXEL_VPTE_TLS sixel_lookup_vpte_cache_t
    sixel_lookup_vpte_thread_cache;

static uint32_t
sixel_lookup_vpte_cache_hash(size_t offset)
{
    uint32_t state;

    state = sixel_lookup_vpte_mix_u32(0x811c9dc5U, (uint32_t)offset);
    state = sixel_lookup_vpte_mix_u32(state, (uint32_t)(offset >> 32));

    return state;
}

static void
sixel_lookup_vpte_cache_clear(sixel_lookup_vpte_cache_t *cache)
{
    size_t set;
    size_t way;

    cache->signature = 0U;
    cache->shared = NULL;
    for (set = 0U; set < 16U; ++set) {
        cache->sets[set].hand = 0U;
        for (way = 0U; way < 4U; ++way) {
            cache->sets[set].key[way] = UINT32_MAX;
            cache->sets[set].value[way] = -1;
        }
    }
}

static void
sixel_lookup_vpte_cache_prepare(sixel_lookup_vpte_shared_t const *shared)
{
    if (sixel_lookup_vpte_thread_cache.shared != shared
        || sixel_lookup_vpte_thread_cache.signature != shared->signature) {
        sixel_lookup_vpte_cache_clear(&sixel_lookup_vpte_thread_cache);
        sixel_lookup_vpte_thread_cache.shared = shared;
        sixel_lookup_vpte_thread_cache.signature = shared->signature;
    }
}

static int
sixel_lookup_vpte_cache_get(sixel_lookup_vpte_cache_t *cache,
                            size_t offset,
                            int *value_out)
{
    uint32_t key;
    size_t set;
    size_t way;

    key = sixel_lookup_vpte_cache_hash(offset);
    set = (size_t)(key & 15U);
    for (way = 0U; way < 4U; ++way) {
        if (cache->sets[set].key[way] == key) {
            *value_out = cache->sets[set].value[way];

            return 1;
        }
    }

    return 0;
}

static void
sixel_lookup_vpte_cache_put(sixel_lookup_vpte_cache_t *cache,
                            size_t offset,
                            int value)
{
    uint32_t key;
    size_t set;
    size_t way;

    key = sixel_lookup_vpte_cache_hash(offset);
    set = (size_t)(key & 15U);
    way = cache->sets[set].hand;
    cache->sets[set].key[way] = key;
    cache->sets[set].value[way] = value;
    cache->sets[set].hand = (uint8_t)((way + 1U) & 3U);
}

uint32_t
sixel_lookup_vpte_float32_signature(float const *palette,
                                    int ncolors,
                                    int resolution,
                                    int refine,
                                    int wcomp1,
                                    int wcomp2,
                                    int wcomp3,
                                    int depth)
{
    uint32_t hash;
    uint32_t bits;
    size_t total;
    size_t index;
    float clamped;

    hash = 0x811c9dc5U;
    hash = sixel_lookup_vpte_mix_u32(hash, (uint32_t)resolution);
    hash = sixel_lookup_vpte_mix_u32(hash, (uint32_t)refine);
    hash = sixel_lookup_vpte_mix_u32(hash, (uint32_t)ncolors);
    hash = sixel_lookup_vpte_mix_u32(hash, (uint32_t)depth);
    hash = sixel_lookup_vpte_mix_u32(hash, (uint32_t)wcomp1);
    hash = sixel_lookup_vpte_mix_u32(hash, (uint32_t)wcomp2);
    hash = sixel_lookup_vpte_mix_u32(hash, (uint32_t)wcomp3);

    total = (size_t)ncolors * (size_t)depth;
    for (index = 0U; index < total; ++index) {
        clamped = palette[index];
        if (clamped < 0.0f) {
            clamped = 0.0f;
        } else if (clamped > 1.0f) {
            clamped = 1.0f;
        }
        memcpy(&bits, &clamped, sizeof(bits));
        hash = sixel_lookup_vpte_mix_u32(hash, bits);
    }

    return hash;
}

uint32_t
sixel_lookup_vpte_float32_shared_signature(
    sixel_lookup_vpte_shared_t const *shared)
{
    if (shared == NULL) {
        return 0U;
    }

    return shared->signature;
}

void
sixel_lookup_vpte_float32_shared_set_signature(
    sixel_lookup_vpte_shared_t *shared,
    uint32_t signature)
{
    if (shared == NULL) {
        return;
    }

    shared->signature = signature;
}

static void
sixel_lookup_vpte_shared_release_palette(sixel_allocator_t *allocator,
                                         sixel_lookup_vpte_shared_t *shared)
{
    if (shared->palette != NULL) {
        sixel_allocator_free(allocator, shared->palette);
        shared->palette = NULL;
    }
    if (shared->palette_quant != NULL) {
        sixel_allocator_free(allocator, shared->palette_quant);
        shared->palette_quant = NULL;
    }
}

static void
sixel_lookup_vpte_shared_release_indices(sixel_allocator_t *allocator,
                                         sixel_lookup_vpte_shared_t *shared)
{
    if (shared->dist2 != NULL) {
        sixel_allocator_free(allocator, shared->dist2);
        shared->dist2 = NULL;
    }
    if (shared->indices8 != NULL) {
        sixel_allocator_free(allocator, shared->indices8);
        shared->indices8 = NULL;
    }
    if (shared->indices16 != NULL) {
        sixel_allocator_free(allocator, shared->indices16);
        shared->indices16 = NULL;
    }
    if (shared->boundary != NULL) {
        sixel_allocator_free(allocator, shared->boundary);
        shared->boundary = NULL;
    }
}

static void
sixel_lookup_vpte_shared_destroy(sixel_allocator_t *allocator,
                                 sixel_lookup_vpte_shared_t *shared)
{
    if (shared == NULL) {
        return;
    }

    sixel_lookup_vpte_shared_release_palette(allocator, shared);
    sixel_lookup_vpte_shared_release_indices(allocator, shared);
    sixel_allocator_free(allocator, shared);
}

static void
sixel_lookup_vpte_shared_unref(sixel_allocator_t *allocator,
                               sixel_lookup_vpte_shared_t *shared)
{
    unsigned int previous;

    if (shared == NULL) {
        return;
    }

    previous = sixel_atomic_fetch_sub_u32(&shared->refcount, 1U);
    if (previous == 1U) {
        sixel_fence_acquire();
        sixel_lookup_vpte_shared_destroy(allocator, shared);
    }
}

static void
sixel_lookup_vpte_shared_ref(sixel_lookup_vpte_shared_t *shared)
{
    if (shared == NULL) {
        return;
    }

    sixel_atomic_fetch_add_u32(&shared->refcount, 1U);
}

static int
sixel_lookup_vpte_palette_index(int depth, int index, int component)
{
    return index * depth + component;
}

static void
sixel_lookup_vpte_quantize_palette(float const *palette,
                                   sixel_lookup_vpte_shared_t *shared)
{
    int index;
    int component;
    float sample;
    int quantized;
    int limit;

    limit = shared->resolution - 1;
    for (index = 0; index < shared->ncolors; ++index) {
        for (component = 0; component < shared->depth; ++component) {
            sample = palette[sixel_lookup_vpte_palette_index(shared->depth,
                                                             index,
                                                             component)];
            if (sample < 0.0f) {
                sample = 0.0f;
            }
            if (sample > 1.0f) {
                sample = 1.0f;
            }
            quantized = (int)lroundf(sample * (float)limit);
            if (quantized < 0) {
                quantized = 0;
            }
            if (quantized > limit) {
                quantized = limit;
            }
            shared->palette_quant[sixel_lookup_vpte_palette_index(shared->depth,
                                                                  index,
                                                                  component)]
                = (unsigned char)quantized;
            shared->palette[sixel_lookup_vpte_palette_index(shared->depth,
                                                            index,
                                                            component)]
                = sample;
        }
    }
}

static void
sixel_lookup_vpte_seed_grid(int resolution,
                            int depth,
                            int ncolors,
                            unsigned char const *palette_quant,
                            double *distances,
                            int *sources)
{
    size_t plane;
    size_t grid;
    int index;
    size_t offset;
    int x;
    int y;
    int z;

    grid = (size_t)resolution * (size_t)resolution * (size_t)resolution;
    for (offset = 0; offset < grid; ++offset) {
        distances[offset] = DBL_MAX / 4.0;
        sources[offset] = -1;
    }

    for (index = 0; index < ncolors; ++index) {
        x = palette_quant[sixel_lookup_vpte_palette_index(depth, index, 0)];
        y = palette_quant[sixel_lookup_vpte_palette_index(depth, index, 1)];
        z = palette_quant[sixel_lookup_vpte_palette_index(depth, index, 2)];
        plane = (size_t)resolution * (size_t)resolution;
        offset = ((size_t)z * plane) + ((size_t)y * (size_t)resolution)
               + (size_t)x;
        distances[offset] = 0.0;
        sources[offset] = index;
    }
}

static void
sixel_lookup_vpte_edt1d(double *line_dist,
                        int *line_src,
                        int length,
                        double weight)
{
    double zbuf[257];
    int vbuf[256];
    double scratch[256];
    int k;
    int q;
    int i;
    double s;
    double candidate;
    double denom;

    vbuf[0] = 0;
    zbuf[0] = -DBL_MAX;
    zbuf[1] = DBL_MAX;
    k = 0;

    for (q = 1; q < length; ++q) {
        denom = 2.0 * weight * (double)(q - vbuf[k]);
        if (denom == 0.0) {
            denom = 1.0;
        }
        candidate = (line_dist[q] + weight * (double)(q * q))
                  - (line_dist[vbuf[k]]
                     + weight * (double)(vbuf[k] * vbuf[k]));
        s = candidate / denom;
        while (s <= zbuf[k]) {
            --k;
            denom = 2.0 * weight * (double)(q - vbuf[k]);
            if (denom == 0.0) {
                denom = 1.0;
            }
            candidate = (line_dist[q] + weight * (double)(q * q))
                      - (line_dist[vbuf[k]]
                         + weight * (double)(vbuf[k] * vbuf[k]));
            s = candidate / denom;
        }
        ++k;
        vbuf[k] = q;
        zbuf[k] = s;
        zbuf[k + 1] = DBL_MAX;
    }

    k = 0;
    for (i = 0; i < length; ++i) {
        while (zbuf[k + 1] < (double)i) {
            ++k;
        }
        scratch[i] = line_dist[vbuf[k]]
                   + weight * (double)((i - vbuf[k]) * (i - vbuf[k]));
        line_src[i] = line_src[vbuf[k]];
    }

    for (i = 0; i < length; ++i) {
        line_dist[i] = scratch[i];
    }
}

static void
sixel_lookup_vpte_apply_edt(sixel_lookup_vpte_shared_t *shared,
                            double *distances,
                            int *sources)
{
    int res;
    size_t plane;
    double weight;
    double line_dist[256];
    int line_src[256];
    int x;
    int y;
    int z;
    size_t offset;
    size_t stride_y;
    size_t stride_z;

    res = shared->resolution;
    plane = (size_t)res * (size_t)res;
    stride_y = (size_t)res;
    stride_z = plane;

    weight = (double)shared->weights[0];
    for (z = 0; z < res; ++z) {
        for (y = 0; y < res; ++y) {
            offset = ((size_t)z * stride_z) + ((size_t)y * stride_y);
            for (x = 0; x < res; ++x) {
                line_dist[x] = distances[offset + (size_t)x];
                line_src[x] = sources[offset + (size_t)x];
            }
            sixel_lookup_vpte_edt1d(line_dist, line_src, res, weight);
            for (x = 0; x < res; ++x) {
                distances[offset + (size_t)x] = line_dist[x];
                sources[offset + (size_t)x] = line_src[x];
            }
        }
    }

    weight = (double)shared->weights[1];
    for (z = 0; z < res; ++z) {
        for (x = 0; x < res; ++x) {
            for (y = 0; y < res; ++y) {
                offset = ((size_t)z * stride_z)
                       + ((size_t)y * stride_y)
                       + (size_t)x;
                line_dist[y] = distances[offset];
                line_src[y] = sources[offset];
            }
            sixel_lookup_vpte_edt1d(line_dist, line_src, res, weight);
            for (y = 0; y < res; ++y) {
                offset = ((size_t)z * stride_z)
                       + ((size_t)y * stride_y)
                       + (size_t)x;
                distances[offset] = line_dist[y];
                sources[offset] = line_src[y];
            }
        }
    }

    weight = (double)shared->weights[2];
    for (y = 0; y < res; ++y) {
        for (x = 0; x < res; ++x) {
            for (z = 0; z < res; ++z) {
                offset = ((size_t)z * stride_z)
                       + ((size_t)y * stride_y)
                       + (size_t)x;
                line_dist[z] = distances[offset];
                line_src[z] = sources[offset];
            }
            sixel_lookup_vpte_edt1d(line_dist, line_src, res, weight);
            for (z = 0; z < res; ++z) {
                offset = ((size_t)z * stride_z)
                       + ((size_t)y * stride_y)
                       + (size_t)x;
                distances[offset] = line_dist[z];
                sources[offset] = line_src[z];
            }
        }
    }
}

static void
sixel_lookup_vpte_fill_indices(sixel_lookup_vpte_shared_t *shared,
                               int *sources)
{
    size_t total;
    size_t index;

    total = (size_t)shared->resolution * (size_t)shared->resolution
          * (size_t)shared->resolution;
    if (!shared->use_u16) {
        for (index = 0; index < total; ++index) {
            shared->indices8[index] = (uint8_t)sources[index];
        }
    } else {
        for (index = 0; index < total; ++index) {
            shared->indices16[index] = (uint16_t)sources[index];
        }
    }
}

static void
sixel_lookup_vpte_mark_boundaries(sixel_lookup_vpte_shared_t *shared,
                                  int *sources)
{
    size_t plane;
    size_t total;
    size_t offset;
    int res;
    int x;
    int y;
    int z;
    int dx;
    int dy;
    int dz;
    int neighbor;
    int current;
    size_t bit_index;
    size_t byte_index;

    res = shared->resolution;
    plane = (size_t)res * (size_t)res;
    total = (size_t)res * plane;

    for (offset = 0; offset < (total + 7U) / 8U; ++offset) {
        shared->boundary[offset] = 0U;
    }

    for (z = 0; z < res; ++z) {
        for (y = 0; y < res; ++y) {
            for (x = 0; x < res; ++x) {
                offset = ((size_t)z * plane)
                       + ((size_t)y * (size_t)res)
                       + (size_t)x;
                current = sources[offset];
                for (dz = -1; dz <= 1; dz += 2) {
                    if (z + dz < 0 || z + dz >= res) {
                        continue;
                    }
                    neighbor = sources[offset + (size_t)dz * plane];
                    if (neighbor != current) {
                        bit_index = offset;
                        byte_index = bit_index / 8U;
                        shared->boundary[byte_index]
                            |= (unsigned char)(1U
                                               << (bit_index % 8U));
                        break;
                    }
                }
                for (dy = -1; dy <= 1; dy += 2) {
                    if (y + dy < 0 || y + dy >= res) {
                        continue;
                    }
                    neighbor = sources[offset + (size_t)dy * (size_t)res];
                    if (neighbor != current) {
                        bit_index = offset;
                        byte_index = bit_index / 8U;
                        shared->boundary[byte_index]
                            |= (unsigned char)(1U
                                               << (bit_index % 8U));
                        break;
                    }
                }
                for (dx = -1; dx <= 1; dx += 2) {
                    if (x + dx < 0 || x + dx >= res) {
                        continue;
                    }
                    neighbor = sources[offset + (size_t)dx];
                    if (neighbor != current) {
                        bit_index = offset;
                        byte_index = bit_index / 8U;
                        shared->boundary[byte_index]
                            |= (unsigned char)(1U
                                               << (bit_index % 8U));
                        break;
                    }
                }
            }
        }
    }
}

static SIXELSTATUS
sixel_lookup_vpte_build(sixel_lookup_vpte_float32_t *vpte,
                        float const *palette,
                        int ncolors,
                        int resolution,
                        int refine,
                        int use_dist2,
                        int wcomp1,
                        int wcomp2,
                        int wcomp3,
                        int depth)
{
    sixel_lookup_vpte_shared_t *shared;
    double *distances;
    int *sources;
    size_t total;
    size_t palette_size;
    size_t offset;

    shared = sixel_allocator_malloc(vpte->allocator, sizeof(*shared));
    if (shared == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vpte_build: allocation failed (shared).");
        return SIXEL_BAD_ALLOCATION;
    }

    shared->refcount = 1U;
    shared->resolution = resolution;
    shared->refine = refine;
    shared->use_dist2 = use_dist2;
    shared->weights[0] = wcomp1;
    shared->weights[1] = wcomp2;
    shared->weights[2] = wcomp3;
    shared->ncolors = ncolors;
    shared->depth = depth;
    shared->safe_radius2 = 0.0;
    shared->use_u16 = ncolors > 256 ? 1 : 0;
    shared->dist2 = NULL;
    shared->indices8 = NULL;
    shared->indices16 = NULL;
    shared->boundary = NULL;
    shared->palette = NULL;
    shared->palette_quant = NULL;

    palette_size = (size_t)ncolors * (size_t)depth;
    shared->palette = (float *)sixel_allocator_malloc(vpte->allocator,
                                                      palette_size
                                                      * sizeof(float));
    shared->palette_quant = (unsigned char *)sixel_allocator_malloc(
        vpte->allocator,
        palette_size);
    if (shared->palette == NULL || shared->palette_quant == NULL) {
        sixel_lookup_vpte_shared_destroy(vpte->allocator, shared);
        sixel_helper_set_additional_message(
            "sixel_lookup_vpte_build: palette allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_lookup_vpte_quantize_palette(palette, shared);

    total = (size_t)resolution * (size_t)resolution * (size_t)resolution;
    if (!shared->use_u16) {
        shared->indices8 = (uint8_t *)sixel_allocator_malloc(vpte->allocator,
                                                             total);
    } else {
        shared->indices16 = (uint16_t *)sixel_allocator_malloc(
            vpte->allocator,
            total * sizeof(uint16_t));
    }
    if (use_dist2 != 0) {
        shared->dist2 = (float *)sixel_allocator_malloc(vpte->allocator,
                                                        total
                                                        * sizeof(float));
    }
    shared->boundary = (unsigned char *)sixel_allocator_malloc(
        vpte->allocator,
        (total + 7U) / 8U);
    if ((shared->use_u16 && shared->indices16 == NULL)
        || (!shared->use_u16 && shared->indices8 == NULL)
        || shared->boundary == NULL
        || (use_dist2 != 0 && shared->dist2 == NULL)) {
        sixel_lookup_vpte_shared_destroy(vpte->allocator, shared);
        sixel_helper_set_additional_message(
            "sixel_lookup_vpte_build: LUT allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    distances = (double *)sixel_allocator_malloc(
        vpte->allocator,
        total * sizeof(double));
    sources = (int *)sixel_allocator_malloc(vpte->allocator,
                                            total * sizeof(int));
    if (distances == NULL || sources == NULL) {
        sixel_allocator_free(vpte->allocator, distances);
        sixel_allocator_free(vpte->allocator, sources);
        sixel_lookup_vpte_shared_destroy(vpte->allocator, shared);
        sixel_helper_set_additional_message(
            "sixel_lookup_vpte_build: temporary buffer allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    sixel_lookup_vpte_seed_grid(resolution,
                                shared->depth,
                                shared->ncolors,
                                shared->palette_quant,
                                distances,
                                sources);
    sixel_lookup_vpte_apply_edt(shared, distances, sources);
    sixel_lookup_vpte_fill_indices(shared, sources);
    sixel_lookup_vpte_mark_boundaries(shared, sources);
    if (shared->dist2 != NULL) {
        for (offset = 0U; offset < total; ++offset) {
            shared->dist2[offset] = (float)distances[offset];
        }
    }

    sixel_allocator_free(vpte->allocator, distances);
    sixel_allocator_free(vpte->allocator, sources);

    /*
     * Each voxel spans a unit cube in lattice space.
     * The farthest point from the center sits 0.5 units away on every axis,
     * so this radius bounds the squared displacement a pixel can accumulate
     * inside the cell.
     */
    shared->safe_radius2 = ((double)wcomp1 * 0.25)
                         + ((double)wcomp2 * 0.25)
                         + ((double)wcomp3 * 0.25);

    vpte->shared = shared;

    return SIXEL_OK;
}

SIXELSTATUS
sixel_lookup_vpte_float32_create(sixel_allocator_t *allocator,
                                 sixel_lookup_vpte_float32_t **vpte_out)
{
    sixel_lookup_vpte_float32_t *vpte;

    if (allocator == NULL || vpte_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    vpte = sixel_allocator_malloc(allocator, sizeof(*vpte));
    if (vpte == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    vpte->allocator = allocator;
    vpte->shared = NULL;
    vpte->use_cache = 0;
    *vpte_out = vpte;

    return SIXEL_OK;
}

void
sixel_lookup_vpte_float32_unref(sixel_lookup_vpte_float32_t *vpte)
{
    sixel_allocator_t *allocator;

    if (vpte == NULL) {
        return;
    }

    allocator = vpte->allocator;
    sixel_lookup_vpte_shared_unref(allocator, vpte->shared);
    sixel_allocator_free(allocator, vpte);
}

SIXELSTATUS
sixel_lookup_vpte_float32_configure(sixel_lookup_vpte_float32_t *vpte,
                                    float const *palette,
                                    int ncolors,
                                    int resolution,
                                    int refine,
                                    int use_dist2,
                                    int use_cache,
                                    int shared_flag,
                                    int wcomp1,
                                    int wcomp2,
                                    int wcomp3,
                                    int pixelformat)
{
    SIXELSTATUS status;

    (void)pixelformat;

    if (vpte == NULL || palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_lookup_vpte_validate_resolution(resolution)) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vpte_float32_configure: resolution must "
            "be 64/128/256.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (vpte->shared != NULL) {
        sixel_lookup_vpte_shared_unref(vpte->allocator, vpte->shared);
        vpte->shared = NULL;
    }

    status = sixel_lookup_vpte_build(vpte,
                                     palette,
                                     ncolors,
                                     resolution,
                                     refine,
                                     use_dist2,
                                     wcomp1,
                                     wcomp2,
                                     wcomp3,
                                     3);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (shared_flag != 0) {
        sixel_lookup_vpte_shared_ref(vpte->shared);
    }

#if SIXEL_VPTE_TLS_AVAILABLE == 0
    /*
     * Thread-local storage is not supported on this platform.  Disable the
     * VPTE cache to avoid sharing a single cache instance across threads.
     */
    use_cache = 0;
#endif

    vpte->use_cache = use_cache;

    return SIXEL_OK;
}

static int
sixel_lookup_vpte_read_index(sixel_lookup_vpte_shared_t const *shared,
                             size_t offset)
{
    if (!shared->use_u16) {
        return (int)shared->indices8[offset];
    }

    return (int)shared->indices16[offset];
}

static int
sixel_lookup_vpte_boundary_bit(sixel_lookup_vpte_shared_t const *shared,
                               size_t offset)
{
    size_t byte_index;
    size_t bit_index;

    byte_index = offset / 8U;
    bit_index = offset % 8U;

    return (shared->boundary[byte_index] >> bit_index) & 1U;
}

static int
sixel_lookup_vpte_refine_needed(sixel_lookup_vpte_shared_t const *shared,
                                size_t offset)
{
    double dist2;

    if (shared->use_dist2 == 0 || shared->dist2 == NULL) {
        return 1;
    }

    dist2 = (double)shared->dist2[offset];
    if (dist2 <= shared->safe_radius2) {
        return 0;
    }

    return 1;
}

static int
sixel_lookup_vpte_refine_candidates(sixel_lookup_vpte_shared_t const *shared,
                                    float const *pixel,
                                    int x,
                                    int y,
                                    int z)
{
    int best_index;
    double best_distance;
    int corner_x[2];
    int corner_y[2];
    int corner_z[2];
    int cx;
    int cy;
    int cz;
    int idx;
    int candidate;
    double dx;
    double dy;
    double dz;
    double distance;
    int weight1;
    int weight2;
    int weight3;
    float p1;
    float p2;
    float p3;
    int used[8];
    int used_count;
    size_t offset;
    size_t plane;

    corner_x[0] = x;
    corner_x[1] = x + 1;
    if (corner_x[1] >= shared->resolution) {
        corner_x[1] = shared->resolution - 1;
    }
    corner_y[0] = y;
    corner_y[1] = y + 1;
    if (corner_y[1] >= shared->resolution) {
        corner_y[1] = shared->resolution - 1;
    }
    corner_z[0] = z;
    corner_z[1] = z + 1;
    if (corner_z[1] >= shared->resolution) {
        corner_z[1] = shared->resolution - 1;
    }

    plane = (size_t)shared->resolution * (size_t)shared->resolution;
    weight1 = shared->weights[0];
    weight2 = shared->weights[1];
    weight3 = shared->weights[2];
    best_index = -1;
    best_distance = DBL_MAX;
    used_count = 0;
    for (cz = 0; cz < 2; ++cz) {
        for (cy = 0; cy < 2; ++cy) {
            for (cx = 0; cx < 2; ++cx) {
                offset = ((size_t)corner_z[cz] * plane)
                       + ((size_t)corner_y[cy] * (size_t)shared->resolution)
                       + (size_t)corner_x[cx];
                candidate = sixel_lookup_vpte_read_index(shared, offset);

                for (idx = 0; idx < used_count; ++idx) {
                    if (used[idx] == candidate) {
                        candidate = -1;
                        break;
                    }
                }
                if (candidate < 0) {
                    continue;
                }
                used[used_count] = candidate;
                ++used_count;

                p1 = shared->palette[sixel_lookup_vpte_palette_index(
                    shared->depth,
                    candidate,
                    0)];
                p2 = shared->palette[sixel_lookup_vpte_palette_index(
                    shared->depth,
                    candidate,
                    1)];
                p3 = shared->palette[sixel_lookup_vpte_palette_index(
                    shared->depth,
                    candidate,
                    2)];
                dx = (double)(pixel[0] - p1);
                dy = (double)(pixel[1] - p2);
                dz = (double)(pixel[2] - p3);
                distance = (double)weight1 * dx * dx
                         + (double)weight2 * dy * dy
                         + (double)weight3 * dz * dz;
                if (distance < best_distance) {
                    best_distance = distance;
                    best_index = candidate;
                }
            }
        }
    }

    return best_index;
}

int
sixel_lookup_vpte_float32_map(sixel_lookup_vpte_float32_t *vpte,
                              float const *pixel)
{
    int x;
    int y;
    int z;
    int limit;
    int cached_value;
    int should_refine;
    int index;
    size_t offset;
    size_t plane;
    float scaled;

    if (vpte == NULL || pixel == NULL || vpte->shared == NULL) {
        return -1;
    }

    limit = vpte->shared->resolution - 1;
    scaled = pixel[0] * (float)limit;
    if (scaled < 0.0f) {
        scaled = 0.0f;
    }
    x = (int)lroundf(scaled);
    if (x > limit) {
        x = limit;
    }
    scaled = pixel[1] * (float)limit;
    if (scaled < 0.0f) {
        scaled = 0.0f;
    }
    y = (int)lroundf(scaled);
    if (y > limit) {
        y = limit;
    }
    scaled = pixel[2] * (float)limit;
    if (scaled < 0.0f) {
        scaled = 0.0f;
    }
    z = (int)lroundf(scaled);
    if (z > limit) {
        z = limit;
    }

    plane = (size_t)vpte->shared->resolution * (size_t)vpte->shared->resolution;
    offset = ((size_t)z * plane)
           + ((size_t)y * (size_t)vpte->shared->resolution)
           + (size_t)x;

    if (vpte->use_cache != 0) {
        sixel_lookup_vpte_cache_prepare(vpte->shared);
        if (sixel_lookup_vpte_cache_get(&sixel_lookup_vpte_thread_cache,
                                         offset,
                                         &cached_value)) {
            return cached_value;
        }
    }

    index = sixel_lookup_vpte_read_index(vpte->shared, offset);
    if (vpte->shared->refine != 0
        && sixel_lookup_vpte_boundary_bit(vpte->shared, offset) != 0) {
        should_refine = sixel_lookup_vpte_refine_needed(vpte->shared, offset);
        if (should_refine != 0) {
            index = sixel_lookup_vpte_refine_candidates(vpte->shared,
                                                        pixel,
                                                        x,
                                                        y,
                                                        z);
        }
    }

    if (vpte->use_cache != 0) {
        sixel_lookup_vpte_cache_put(&sixel_lookup_vpte_thread_cache,
                                    offset,
                                    index);
    }

    return index;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
