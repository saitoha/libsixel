/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

/*
 * Benchmark decoder-side fast4 dequantization when the CPU has already
 * materialized RGBA pixels.  The RGBA Metal kernel deliberately stays in RGB
 * color space and computes the k_undither-style neighbour weight from the
 * center color, neighbour color, and palette.  It does not recover palette
 * indices from RGBA, because that would add work that an RGBA handoff does not
 * need and would be ambiguous when the palette contains duplicate RGB values.
 * The indexed Metal kernel is included as an upper-bound comparison for a
 * future API that keeps indices visible.
 *
 * The CSV still reports decode_raw_ms because the benchmark needs indexed and
 * mask metadata for the CPU indexed baseline and for the indexed Metal upper
 * bound.  The RGBA shader only uses RGBA alpha to skip transparent pixels; its
 * production input is direct RGBA plus palette metadata.  The RGBA+Metal
 * estimate is decode_direct_rgba_ms plus metal_rgba_direct_ms; it assumes an
 * internal path that keeps the palette while producing direct RGBA, not two
 * public decoder calls.
 */

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sixel.h>

#include "decoder.h"
#include "sixel_decode_pixels.h"
#include "threading.h"

typedef struct bench_params {
    uint32_t pixel_count;
    uint32_t width;
    uint32_t height;
    uint32_t ncolors;
    uint32_t has_mask;
} bench_params_t;

typedef struct bench_metal {
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLComputePipelineState> indexed_pipeline;
    id<MTLComputePipelineState> rgba_pipeline;
} bench_metal_t;

typedef struct bench_image {
    unsigned char *data;
    size_t data_size;
    unsigned char *indexed;
    unsigned char *mask;
    unsigned char *palette;
    unsigned char *rgba;
    unsigned char *weights;
    unsigned char *cpu_reference;
    int width;
    int height;
    int ncolors;
} bench_image_t;

typedef struct bench_metal_buffers {
    id<MTLBuffer> result;
    id<MTLBuffer> indexed;
    id<MTLBuffer> rgba;
    id<MTLBuffer> mask;
    id<MTLBuffer> palette;
    id<MTLBuffer> weights;
    id<MTLBuffer> params;
} bench_metal_buffers_t;

static char const * const g_metal_source =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct Params {\n"
"    uint pixel_count;\n"
"    uint width;\n"
"    uint height;\n"
"    uint ncolors;\n"
"    uint has_mask;\n"
"};\n"
"static uint color_diff(uchar3 a, uchar3 b)\n"
"{\n"
"    int dr = int(a.x) - int(b.x);\n"
"    int dg = int(a.y) - int(b.y);\n"
"    int db = int(a.z) - int(b.z);\n"
"    return uint(dr * dr + dg * dg + db * db);\n"
"}\n"
"static bool same_color(uchar3 a, uchar3 b)\n"
"{\n"
"    return all(a == b);\n"
"}\n"
"static uint similarity_weight_direct(device const uchar *palette,\n"
"                                     constant Params& params,\n"
"                                     uchar3 center,\n"
"                                     uchar3 neighbor)\n"
"{\n"
"    uchar3 avg;\n"
"    uint distance;\n"
"    uint base_distance;\n"
"    uint min_diff = 0xffffffffu;\n"
"    if (same_color(center, neighbor)) {\n"
"        return 7u;\n"
"    }\n"
"    avg = uchar3(uchar((uint(center.x) + uint(neighbor.x)) >> 1),\n"
"                 uchar((uint(center.y) + uint(neighbor.y)) >> 1),\n"
"                 uchar((uint(center.z) + uint(neighbor.z)) >> 1));\n"
"    distance = color_diff(avg, center);\n"
"    base_distance = distance;\n"
"    if (base_distance == 0u) {\n"
"        base_distance = 1u;\n"
"    }\n"
"    for (uint i = 0u; i < params.ncolors; ++i) {\n"
"        uchar3 p = uchar3(palette[i * 3u + 0u],\n"
"                          palette[i * 3u + 1u],\n"
"                          palette[i * 3u + 2u]);\n"
"        uint diff;\n"
"        if (same_color(p, center) || same_color(p, neighbor)) {\n"
"            continue;\n"
"        }\n"
"        diff = color_diff(avg, p);\n"
"        if (diff < min_diff) {\n"
"            min_diff = diff;\n"
"        }\n"
"    }\n"
"    if (min_diff == 0xffffffffu) {\n"
"        min_diff = base_distance * 2u;\n"
"    }\n"
"    if (min_diff >= base_distance * 2u) {\n"
"        return 5u;\n"
"    }\n"
"    if (min_diff >= base_distance) {\n"
"        return 8u;\n"
"    }\n"
"    if (min_diff * 6u >= base_distance * 5u) {\n"
"        return 7u;\n"
"    }\n"
"    if (min_diff * 4u >= base_distance * 3u) {\n"
"        return 7u;\n"
"    }\n"
"    if (min_diff * 3u >= base_distance * 2u) {\n"
"        return 5u;\n"
"    }\n"
"    if (min_diff * 5u >= base_distance * 3u) {\n"
"        return 7u;\n"
"    }\n"
"    if (min_diff * 2u >= base_distance) {\n"
"        return 4u;\n"
"    }\n"
"    if (min_diff * 3u >= base_distance) {\n"
"        return 2u;\n"
"    }\n"
"    return 0u;\n"
"}\n"
"static bool pixel_visible(device const uchar *mask,\n"
"                          constant Params& params,\n"
"                          uint pos)\n"
"{\n"
"    return params.has_mask == 0u || mask[pos] != 0u;\n"
"}\n"
"static void store_transparent(device uchar *result, uint gid)\n"
"{\n"
"    result[gid * 4u + 0u] = uchar(0);\n"
"    result[gid * 4u + 1u] = uchar(0);\n"
"    result[gid * 4u + 2u] = uchar(0);\n"
"    result[gid * 4u + 3u] = uchar(0);\n"
"}\n"
"static void store_filtered(device uchar *result,\n"
"                           uint gid,\n"
"                           uint accum_r,\n"
"                           uint accum_g,\n"
"                           uint accum_b,\n"
"                           uint total)\n"
"{\n"
"    result[gid * 4u + 0u] = uchar(accum_r / total);\n"
"    result[gid * 4u + 1u] = uchar(accum_g / total);\n"
"    result[gid * 4u + 2u] = uchar(accum_b / total);\n"
"    result[gid * 4u + 3u] = uchar(255);\n"
"}\n"
"kernel void fast4_indexed(\n"
"    device uchar *result [[buffer(0)]],\n"
"    device const uchar *indexed [[buffer(1)]],\n"
"    device const uchar *rgba [[buffer(2)]],\n"
"    device const uchar *mask [[buffer(3)]],\n"
"    device const uchar *palette [[buffer(4)]],\n"
"    device const uchar *weights [[buffer(5)]],\n"
"    constant Params& params [[buffer(6)]],\n"
"    uint gid [[thread_position_in_grid]])\n"
"{\n"
"    const int2 offsets[4] = {\n"
"        int2(-1, -1), int2(0, -1), int2(1, -1), int2(-1, 0)\n"
"    };\n"
"    uint x;\n"
"    uint y;\n"
"    uint center;\n"
"    uint accum_r;\n"
"    uint accum_g;\n"
"    uint accum_b;\n"
"    uint total;\n"
"    if (gid >= params.pixel_count) {\n"
"        return;\n"
"    }\n"
"    if (!pixel_visible(mask, params, gid)) {\n"
"        store_transparent(result, gid);\n"
"        return;\n"
"    }\n"
"    x = gid % params.width;\n"
"    y = gid / params.width;\n"
"    center = uint(indexed[gid]);\n"
"    if (center >= params.ncolors) {\n"
"        center = 0u;\n"
"    }\n"
"    accum_r = uint(palette[center * 3u + 0u]) * 8u;\n"
"    accum_g = uint(palette[center * 3u + 1u]) * 8u;\n"
"    accum_b = uint(palette[center * 3u + 2u]) * 8u;\n"
"    total = 8u;\n"
"    for (uint n = 0u; n < 4u; ++n) {\n"
"        int nx = int(x) + offsets[n].x;\n"
"        int ny = int(y) + offsets[n].y;\n"
"        uint pos;\n"
"        uint neighbor;\n"
"        uint weight;\n"
"        if (nx < 0 || ny < 0 || nx >= int(params.width) ||\n"
"            ny >= int(params.height)) {\n"
"            continue;\n"
"        }\n"
"        pos = uint(ny) * params.width + uint(nx);\n"
"        if (!pixel_visible(mask, params, pos)) {\n"
"            continue;\n"
"        }\n"
"        neighbor = uint(indexed[pos]);\n"
"        if (neighbor >= params.ncolors) {\n"
"            continue;\n"
"        }\n"
"        weight = uint(weights[center * params.ncolors + neighbor]);\n"
"        accum_r += uint(palette[neighbor * 3u + 0u]) * weight;\n"
"        accum_g += uint(palette[neighbor * 3u + 1u]) * weight;\n"
"        accum_b += uint(palette[neighbor * 3u + 2u]) * weight;\n"
"        total += weight;\n"
"    }\n"
"    store_filtered(result, gid, accum_r, accum_g, accum_b, total);\n"
"}\n"
"kernel void fast4_rgba(\n"
"    device uchar *result [[buffer(0)]],\n"
"    device const uchar *indexed [[buffer(1)]],\n"
"    device const uchar *rgba [[buffer(2)]],\n"
"    device const uchar *mask [[buffer(3)]],\n"
"    device const uchar *palette [[buffer(4)]],\n"
"    device const uchar *weights [[buffer(5)]],\n"
"    constant Params& params [[buffer(6)]],\n"
"    uint gid [[thread_position_in_grid]])\n"
"{\n"
"    const int2 offsets[4] = {\n"
"        int2(-1, -1), int2(0, -1), int2(1, -1), int2(-1, 0)\n"
"    };\n"
"    uint x;\n"
"    uint y;\n"
"    uchar3 center_color;\n"
"    uint accum_r;\n"
"    uint accum_g;\n"
"    uint accum_b;\n"
"    uint total;\n"
"    if (gid >= params.pixel_count) {\n"
"        return;\n"
"    }\n"
"    if (rgba[gid * 4u + 3u] == 0u) {\n"
"        store_transparent(result, gid);\n"
"        return;\n"
"    }\n"
"    x = gid % params.width;\n"
"    y = gid / params.width;\n"
"    center_color = uchar3(rgba[gid * 4u + 0u], rgba[gid * 4u + 1u],\n"
"                          rgba[gid * 4u + 2u]);\n"
"    accum_r = uint(center_color.x) * 8u;\n"
"    accum_g = uint(center_color.y) * 8u;\n"
"    accum_b = uint(center_color.z) * 8u;\n"
"    total = 8u;\n"
"    for (uint n = 0u; n < 4u; ++n) {\n"
"        int nx = int(x) + offsets[n].x;\n"
"        int ny = int(y) + offsets[n].y;\n"
"        uint pos;\n"
"        uchar3 neighbor_color;\n"
"        uint weight;\n"
"        if (nx < 0 || ny < 0 || nx >= int(params.width) ||\n"
"            ny >= int(params.height)) {\n"
"            continue;\n"
"        }\n"
"        pos = uint(ny) * params.width + uint(nx);\n"
"        if (rgba[pos * 4u + 3u] == 0u) {\n"
"            continue;\n"
"        }\n"
"        neighbor_color = uchar3(rgba[pos * 4u + 0u],\n"
"                                rgba[pos * 4u + 1u],\n"
"                                rgba[pos * 4u + 2u]);\n"
"        weight = similarity_weight_direct(palette,\n"
"                                          params,\n"
"                                          center_color,\n"
"                                          neighbor_color);\n"
"        if (weight == 0u) {\n"
"            continue;\n"
"        }\n"
"        accum_r += uint(neighbor_color.x) * weight;\n"
"        accum_g += uint(neighbor_color.y) * weight;\n"
"        accum_b += uint(neighbor_color.z) * weight;\n"
"        total += weight;\n"
"    }\n"
"    store_filtered(result, gid, accum_r, accum_g, accum_b, total);\n"
"}\n";

static double
now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int
read_file(char const *path, unsigned char **data, size_t *size)
{
    FILE *fp;
    long end;
    unsigned char *buffer;
    size_t read_size;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "%s: fopen failed: %s\n", path, strerror(errno));
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "%s: fseek failed\n", path);
        fclose(fp);
        return -1;
    }
    end = ftell(fp);
    if (end < 0) {
        fprintf(stderr, "%s: ftell failed\n", path);
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "%s: rewind failed\n", path);
        fclose(fp);
        return -1;
    }
    buffer = (unsigned char *)malloc((size_t)end);
    if (buffer == NULL) {
        fprintf(stderr, "%s: malloc failed\n", path);
        fclose(fp);
        return -1;
    }
    read_size = fread(buffer, 1, (size_t)end, fp);
    fclose(fp);
    if (read_size != (size_t)end) {
        fprintf(stderr, "%s: short read\n", path);
        free(buffer);
        return -1;
    }
    *data = buffer;
    *size = read_size;
    return 0;
}

static unsigned int
similarity_diff(unsigned char const *a, unsigned char const *b)
{
    int dr;
    int dg;
    int db;

    dr = (int)a[0] - (int)b[0];
    dg = (int)a[1] - (int)b[1];
    db = (int)a[2] - (int)b[2];
    return (unsigned int)(dr * dr + dg * dg + db * db);
}

static unsigned int
similarity_min_diff(unsigned char const *palette,
                    int ncolors,
                    int index1,
                    int index2,
                    unsigned char const *avg)
{
    unsigned int min_diff;
    unsigned int diff;
    unsigned char const *p;
    int i;

    min_diff = UINT_MAX;
    for (i = 0; i < ncolors; ++i) {
        if (i == index1 || i == index2) {
            continue;
        }
        p = palette + i * 3;
        diff = similarity_diff(avg, p);
        if (diff < min_diff) {
            min_diff = diff;
        }
    }
    return min_diff;
}

static unsigned int
similarity_weight(unsigned char const *palette,
                  int ncolors,
                  int index1,
                  int index2,
                  int bias)
{
    unsigned char const *p1;
    unsigned char const *p2;
    unsigned char avg[3];
    unsigned int distance;
    unsigned int base_distance;
    unsigned long long scaled_distance;
    unsigned int min_diff;

    if (index1 == index2) {
        return 7U;
    }
    if (bias < 1) {
        bias = 1;
    }

    p1 = palette + index1 * 3;
    p2 = palette + index2 * 3;
    avg[0] = (unsigned char)(((unsigned int)p1[0]
                              + (unsigned int)p2[0]) >> 1);
    avg[1] = (unsigned char)(((unsigned int)p1[1]
                              + (unsigned int)p2[1]) >> 1);
    avg[2] = (unsigned char)(((unsigned int)p1[2]
                              + (unsigned int)p2[2]) >> 1);

    distance = similarity_diff(avg, p1);
    scaled_distance = (unsigned long long)distance
                    * (unsigned long long)bias + 50ULL;
    base_distance = (unsigned int)(scaled_distance / 100ULL);
    if (base_distance == 0U) {
        base_distance = 1U;
    }

    min_diff = similarity_min_diff(palette, ncolors, index1, index2, avg);
    if (min_diff == UINT_MAX) {
        min_diff = base_distance * 2U;
    }

    if (min_diff >= base_distance * 2U) {
        return 5U;
    }
    if (min_diff >= base_distance) {
        return 8U;
    }
    if ((unsigned long long)min_diff * 6ULL
            >= (unsigned long long)base_distance * 5ULL) {
        return 7U;
    }
    if ((unsigned long long)min_diff * 4ULL
            >= (unsigned long long)base_distance * 3ULL) {
        return 7U;
    }
    if ((unsigned long long)min_diff * 3ULL
            >= (unsigned long long)base_distance * 2ULL) {
        return 5U;
    }
    if ((unsigned long long)min_diff * 5ULL
            >= (unsigned long long)base_distance * 3ULL) {
        return 7U;
    }
    if ((unsigned long long)min_diff * 2ULL
            >= (unsigned long long)base_distance) {
        return 4U;
    }
    if ((unsigned long long)min_diff * 3ULL
            >= (unsigned long long)base_distance) {
        return 2U;
    }
    return 0U;
}

static int
build_weights(bench_image_t *image, int bias)
{
    size_t size;
    signed char *cache;
    int const neighbor_offsets[4][2] = {
        { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 }
    };
    int i;
    int j;
    int x;
    int y;
    int nx;
    int ny;
    int center;
    int neighbor;
    int min_index;
    int max_index;
    int n;
    unsigned int weight;

    size = (size_t)image->ncolors * (size_t)image->ncolors;
    image->weights = (unsigned char *)malloc(size);
    cache = (signed char *)malloc(size);
    if (image->weights == NULL || cache == NULL) {
        free(image->weights);
        free(cache);
        image->weights = NULL;
        return -1;
    }
    memset(cache, -1, size);
    for (i = 0; i < image->ncolors; ++i) {
        cache[(size_t)i * (size_t)image->ncolors + (size_t)i] = 7;
    }

    /*
     * The library's similarity cache is keyed by unordered palette pair, but
     * the midpoint rounding in the historical compare is slightly directional
     * for odd RGB differences.  Match the CPU path by recording the first
     * orientation seen in scalar image order, then mirror that value into the
     * dense table consumed by Metal.
     */
    for (y = 0; y < image->height; ++y) {
        for (x = 0; x < image->width; ++x) {
            if (image->mask != NULL &&
                    image->mask[(size_t)y * (size_t)image->width
                                + (size_t)x] == 0U) {
                continue;
            }
            center = image->indexed[(size_t)y * (size_t)image->width
                                    + (size_t)x];
            if (center < 0 || center >= image->ncolors) {
                center = 0;
            }

            for (n = 0; n < 4; ++n) {
                nx = x + neighbor_offsets[n][0];
                ny = y + neighbor_offsets[n][1];
                if (nx < 0 || ny < 0 || nx >= image->width ||
                        ny >= image->height) {
                    continue;
                }
                if (image->mask != NULL &&
                        image->mask[(size_t)ny * (size_t)image->width
                                    + (size_t)nx] == 0U) {
                    continue;
                }
                neighbor = image->indexed[(size_t)ny * (size_t)image->width
                                          + (size_t)nx];
                if (neighbor < 0 || neighbor >= image->ncolors ||
                        center == neighbor) {
                    continue;
                }
                if (center <= neighbor) {
                    min_index = center;
                    max_index = neighbor;
                } else {
                    min_index = neighbor;
                    max_index = center;
                }
                if (cache[(size_t)min_index * (size_t)image->ncolors
                          + (size_t)max_index] >= 0) {
                    continue;
                }
                weight = similarity_weight(image->palette,
                                           image->ncolors,
                                           center,
                                           neighbor,
                                           bias);
                cache[(size_t)min_index * (size_t)image->ncolors
                      + (size_t)max_index] = (signed char)weight;
            }
        }
    }

    for (i = 0; i < image->ncolors; ++i) {
        for (j = 0; j < image->ncolors; ++j) {
            if (i <= j) {
                min_index = i;
                max_index = j;
            } else {
                min_index = j;
                max_index = i;
            }
            if (cache[(size_t)min_index * (size_t)image->ncolors
                      + (size_t)max_index] < 0) {
                weight = similarity_weight(image->palette,
                                           image->ncolors,
                                           i,
                                           j,
                                           bias);
                cache[(size_t)min_index * (size_t)image->ncolors
                      + (size_t)max_index] = (signed char)weight;
            }
            image->weights[(size_t)i * (size_t)image->ncolors + (size_t)j]
                = (unsigned char)cache[
                    (size_t)min_index * (size_t)image->ncolors
                    + (size_t)max_index];
        }
    }
    free(cache);
    return 0;
}

static int
count_palette_duplicate_colors(bench_image_t const *image)
{
    unsigned char const *a;
    unsigned char const *b;
    int duplicates;
    int i;
    int j;

    duplicates = 0;
    for (i = 0; i < image->ncolors; ++i) {
        a = image->palette + (size_t)i * 3U;
        for (j = 0; j < i; ++j) {
            b = image->palette + (size_t)j * 3U;
            if (a[0] == b[0] && a[1] == b[1] && a[2] == b[2]) {
                ++duplicates;
                break;
            }
        }
    }
    return duplicates;
}

static int
decode_direct_once(bench_image_t *image, sixel_allocator_t *allocator)
{
    unsigned char *work;
    unsigned char *rgba;
    unsigned int result_flags;
    int width;
    int height;
    SIXELSTATUS status;

    work = (unsigned char *)malloc(image->data_size);
    if (work == NULL) {
        return -1;
    }
    memcpy(work, image->data, image->data_size);
    rgba = NULL;
    width = 0;
    height = 0;
    status = sixel_decode_direct_with_options(work,
                                              (int)image->data_size,
                                              0U,
                                              &rgba,
                                              &width,
                                              &height,
                                              &result_flags,
                                              allocator);
    free(work);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sixel_decode_direct_with_options failed: %d\n",
                status);
        return -1;
    }
    if (width != image->width || height != image->height) {
        fprintf(stderr,
                "direct decode size mismatch: raw=%dx%d direct=%dx%d\n",
                image->width,
                image->height,
                width,
                height);
        sixel_allocator_free(allocator, rgba);
        return -1;
    }
    image->rgba = rgba;
    return 0;
}

static int
decode_once(bench_image_t *image, sixel_allocator_t *allocator)
{
    unsigned char *work;
    unsigned int result_flags;
    SIXELSTATUS status;

    work = (unsigned char *)malloc(image->data_size);
    if (work == NULL) {
        return -1;
    }
    memcpy(work, image->data, image->data_size);
    status = sixel_decode_raw_with_options_mask(work,
                                                (int)image->data_size,
                                                0U,
                                                &image->indexed,
                                                &image->mask,
                                                &image->width,
                                                &image->height,
                                                &image->palette,
                                                &image->ncolors,
                                                &result_flags,
                                                allocator);
    free(work);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sixel_decode_raw_with_options_mask failed: %d\n",
                status);
        return -1;
    }
    return 0;
}

static double
bench_decode_raw(bench_image_t const *image,
                 sixel_allocator_t *allocator,
                 int iterations)
{
    double start;
    double total;
    unsigned char *work;
    unsigned char *indexed;
    unsigned char *mask;
    unsigned char *palette;
    unsigned int result_flags;
    int width;
    int height;
    int ncolors;
    int i;
    SIXELSTATUS status;

    total = 0.0;
    for (i = 0; i < iterations; ++i) {
        work = (unsigned char *)malloc(image->data_size);
        if (work == NULL) {
            return -1.0;
        }
        memcpy(work, image->data, image->data_size);
        indexed = NULL;
        mask = NULL;
        palette = NULL;
        start = now_ms();
        status = sixel_decode_raw_with_options_mask(work,
                                                    (int)image->data_size,
                                                    0U,
                                                    &indexed,
                                                    &mask,
                                                    &width,
                                                    &height,
                                                    &palette,
                                                    &ncolors,
                                                    &result_flags,
                                                    allocator);
        total += now_ms() - start;
        free(work);
        if (SIXEL_FAILED(status)) {
            return -1.0;
        }
        sixel_allocator_free(allocator, indexed);
        sixel_allocator_free(allocator, mask);
        sixel_allocator_free(allocator, palette);
    }
    return total / (double)iterations;
}

static double
bench_decode_direct(bench_image_t const *image,
                    sixel_allocator_t *allocator,
                    int iterations)
{
    double start;
    double total;
    unsigned char *work;
    unsigned char *rgba;
    unsigned int result_flags;
    int width;
    int height;
    int i;
    SIXELSTATUS status;

    total = 0.0;
    for (i = 0; i < iterations; ++i) {
        work = (unsigned char *)malloc(image->data_size);
        if (work == NULL) {
            return -1.0;
        }
        memcpy(work, image->data, image->data_size);
        rgba = NULL;
        start = now_ms();
        status = sixel_decode_direct_with_options(work,
                                                  (int)image->data_size,
                                                  0U,
                                                  &rgba,
                                                  &width,
                                                  &height,
                                                  &result_flags,
                                                  allocator);
        total += now_ms() - start;
        free(work);
        if (SIXEL_FAILED(status)) {
            return -1.0;
        }
        sixel_allocator_free(allocator, rgba);
    }
    return total / (double)iterations;
}

static double
bench_cpu_fast4(bench_image_t const *image,
                sixel_allocator_t *allocator,
                int threads,
                int iterations)
{
    double start;
    double total;
    unsigned char *output;
    int i;
    SIXELSTATUS status;

    total = 0.0;
    sixel_set_threads(threads);
    for (i = 0; i < iterations; ++i) {
        output = NULL;
        start = now_ms();
        status = sixel_dequantize_k_undither_fast4_rgba(image->indexed,
                                                        image->mask,
                                                        image->width,
                                                        image->height,
                                                        image->palette,
                                                        image->ncolors,
                                                        100,
                                                        allocator,
                                                        &output);
        total += now_ms() - start;
        if (SIXEL_FAILED(status)) {
            fprintf(stderr, "cpu fast4 failed: %d\n", status);
            return -1.0;
        }
        sixel_allocator_free(allocator, output);
    }
    return total / (double)iterations;
}

static int
make_cpu_reference(bench_image_t *image, sixel_allocator_t *allocator)
{
    SIXELSTATUS status;

    sixel_set_threads(1);
    status = sixel_dequantize_k_undither_fast4_rgba(image->indexed,
                                                    image->mask,
                                                    image->width,
                                                    image->height,
                                                    image->palette,
                                                    image->ncolors,
                                                    100,
                                                    allocator,
                                                    &image->cpu_reference);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "cpu reference fast4 failed: %d\n", status);
        return -1;
    }
    return 0;
}

static int
metal_init(bench_metal_t *metal)
{
    NSError *error;
    NSString *source;
    id<MTLLibrary> library;
    id<MTLFunction> indexed_function;
    id<MTLFunction> rgba_function;

    memset(metal, 0, sizeof(*metal));
    error = nil;
    metal->device = MTLCreateSystemDefaultDevice();
    if (metal->device == nil) {
        fprintf(stderr, "Metal device is not available\n");
        return -1;
    }
    metal->queue = [metal->device newCommandQueue];
    if (metal->queue == nil) {
        fprintf(stderr, "Metal command queue allocation failed\n");
        return -1;
    }
    source = [NSString stringWithUTF8String:g_metal_source];
    library = [metal->device newLibraryWithSource:source
                                          options:nil
                                            error:&error];
    if (library == nil) {
        fprintf(stderr, "Metal library build failed: %s\n",
                [[error localizedDescription] UTF8String]);
        return -1;
    }

    indexed_function = [library newFunctionWithName:@"fast4_indexed"];
    rgba_function = [library newFunctionWithName:@"fast4_rgba"];
    if (indexed_function == nil || rgba_function == nil) {
        fprintf(stderr, "Metal function lookup failed\n");
        return -1;
    }

    error = nil;
    metal->indexed_pipeline =
        [metal->device newComputePipelineStateWithFunction:indexed_function
                                                     error:&error];
    if (metal->indexed_pipeline == nil) {
        fprintf(stderr, "Metal indexed pipeline failed: %s\n",
                [[error localizedDescription] UTF8String]);
        return -1;
    }
    error = nil;
    metal->rgba_pipeline =
        [metal->device newComputePipelineStateWithFunction:rgba_function
                                                     error:&error];
    if (metal->rgba_pipeline == nil) {
        fprintf(stderr, "Metal rgba pipeline failed: %s\n",
                [[error localizedDescription] UTF8String]);
        return -1;
    }
    return 0;
}

static id<MTLBuffer>
new_shared_buffer(id<MTLDevice> device, void const *data, size_t size)
{
    id<MTLBuffer> buffer;

    buffer = [device newBufferWithLength:size
                                 options:MTLResourceStorageModeShared];
    if (buffer == nil) {
        return nil;
    }
    if (data != NULL && size > 0U) {
        memcpy([buffer contents], data, size);
    }
    return buffer;
}

static int
metal_buffers_init(bench_metal_t const *metal,
                   bench_image_t const *image,
                   bench_metal_buffers_t *buffers)
{
    bench_params_t params;
    size_t pixels;
    unsigned char dummy_mask;

    memset(buffers, 0, sizeof(*buffers));
    pixels = (size_t)image->width * (size_t)image->height;
    dummy_mask = 0xffU;
    params.pixel_count = (uint32_t)pixels;
    params.width = (uint32_t)image->width;
    params.height = (uint32_t)image->height;
    params.ncolors = (uint32_t)image->ncolors;
    params.has_mask = image->mask != NULL ? 1U : 0U;

    buffers->result = new_shared_buffer(metal->device, NULL, pixels * 4U);
    buffers->indexed = new_shared_buffer(metal->device,
                                         image->indexed,
                                         pixels);
    buffers->rgba = new_shared_buffer(metal->device,
                                      image->rgba,
                                      pixels * 4U);
    buffers->mask = new_shared_buffer(metal->device,
                                      image->mask != NULL
                                          ? image->mask : &dummy_mask,
                                      image->mask != NULL ? pixels : 1U);
    buffers->palette = new_shared_buffer(metal->device,
                                         image->palette,
                                         (size_t)image->ncolors * 3U);
    buffers->weights = new_shared_buffer(
        metal->device,
        image->weights,
        (size_t)image->ncolors * (size_t)image->ncolors);
    buffers->params = new_shared_buffer(metal->device,
                                        &params,
                                        sizeof(params));

    if (buffers->result == nil || buffers->indexed == nil ||
            buffers->rgba == nil || buffers->mask == nil ||
            buffers->palette == nil || buffers->weights == nil ||
            buffers->params == nil) {
        fprintf(stderr, "Metal buffer allocation failed\n");
        return -1;
    }
    return 0;
}

static int
metal_run_once(bench_metal_t const *metal,
               bench_metal_buffers_t const *buffers,
               int use_rgba)
{
    id<MTLCommandBuffer> command;
    id<MTLComputeCommandEncoder> encoder;
    id<MTLComputePipelineState> pipeline;
    MTLSize grid;
    MTLSize group;
    NSUInteger width;

    pipeline = use_rgba ? metal->rgba_pipeline : metal->indexed_pipeline;
    command = [metal->queue commandBuffer];
    if (command == nil) {
        return -1;
    }
    encoder = [command computeCommandEncoder];
    if (encoder == nil) {
        return -1;
    }
    [encoder setComputePipelineState:pipeline];
    [encoder setBuffer:buffers->result offset:0 atIndex:0];
    [encoder setBuffer:buffers->indexed offset:0 atIndex:1];
    [encoder setBuffer:buffers->rgba offset:0 atIndex:2];
    [encoder setBuffer:buffers->mask offset:0 atIndex:3];
    [encoder setBuffer:buffers->palette offset:0 atIndex:4];
    [encoder setBuffer:buffers->weights offset:0 atIndex:5];
    [encoder setBuffer:buffers->params offset:0 atIndex:6];
    width = [pipeline threadExecutionWidth];
    if (width < 1U) {
        width = 64U;
    }
    grid = MTLSizeMake(((bench_params_t *)[buffers->params contents])
                           ->pixel_count,
                       1,
                       1);
    group = MTLSizeMake(width, 1, 1);
    [encoder dispatchThreads:grid threadsPerThreadgroup:group];
    [encoder endEncoding];
    [command commit];
    [command waitUntilCompleted];
    if ([command status] != MTLCommandBufferStatusCompleted) {
        return -1;
    }
    return 0;
}

static double
bench_metal_run(bench_metal_t const *metal,
                bench_metal_buffers_t const *buffers,
                unsigned char *readback,
                size_t readback_size,
                int use_rgba,
                int include_readback,
                int iterations)
{
    double start;
    double total;
    int i;

    total = 0.0;
    for (i = 0; i < iterations; ++i) {
        start = now_ms();
        if (metal_run_once(metal, buffers, use_rgba) != 0) {
            return -1.0;
        }
        if (include_readback) {
            memcpy(readback, [buffers->result contents], readback_size);
        }
        total += now_ms() - start;
    }
    return total / (double)iterations;
}

static int
compare_rgba(unsigned char const *expected,
             unsigned char const *actual,
             size_t bytes,
             unsigned int *max_abs_diff)
{
    unsigned int max_diff;
    unsigned int diff;
    size_t mismatch;
    size_t i;

    max_diff = 0U;
    mismatch = 0U;
    for (i = 0; i < bytes; ++i) {
        diff = expected[i] > actual[i]
             ? (unsigned int)expected[i] - (unsigned int)actual[i]
             : (unsigned int)actual[i] - (unsigned int)expected[i];
        if (diff != 0U) {
            ++mismatch;
            if (diff > max_diff) {
                max_diff = diff;
            }
        }
    }
    *max_abs_diff = max_diff;
    return mismatch == 0U ? 0 : -1;
}

static void
image_free(bench_image_t *image, sixel_allocator_t *allocator)
{
    if (image->data != NULL) {
        free(image->data);
    }
    if (image->indexed != NULL) {
        sixel_allocator_free(allocator, image->indexed);
    }
    if (image->mask != NULL) {
        sixel_allocator_free(allocator, image->mask);
    }
    if (image->palette != NULL) {
        sixel_allocator_free(allocator, image->palette);
    }
    if (image->rgba != NULL) {
        sixel_allocator_free(allocator, image->rgba);
    }
    if (image->weights != NULL) {
        free(image->weights);
    }
    if (image->cpu_reference != NULL) {
        sixel_allocator_free(allocator, image->cpu_reference);
    }
    memset(image, 0, sizeof(*image));
}

static int
bench_path(char const *path,
           bench_metal_t const *metal,
           sixel_allocator_t *allocator,
           int iterations)
{
    bench_image_t image;
    bench_metal_buffers_t buffers;
    unsigned char *readback;
    size_t pixels;
    size_t rgba_bytes;
    double decode_raw_ms;
    double decode_direct_rgba_ms;
    double cpu1_ms;
    double cpu2_ms;
    double cpu4_ms;
    double cpu8_ms;
    double metal_indexed_ms;
    double metal_rgba_direct_ms;
    double metal_rgba_direct_readback_ms;
    unsigned int indexed_max_diff;
    unsigned int rgba_max_diff;
    int duplicate_colors;
    int indexed_ok;
    int rgba_ok;
    int status;

    memset(&image, 0, sizeof(image));
    memset(&buffers, 0, sizeof(buffers));
    readback = NULL;
    status = -1;
    if (read_file(path, &image.data, &image.data_size) != 0) {
        goto end;
    }
    decode_raw_ms = bench_decode_raw(&image, allocator, iterations);
    if (decode_raw_ms < 0.0 || decode_once(&image, allocator) != 0) {
        goto end;
    }
    decode_direct_rgba_ms = bench_decode_direct(&image,
                                                allocator,
                                                iterations);
    if (decode_direct_rgba_ms < 0.0 ||
            decode_direct_once(&image, allocator) != 0) {
        goto end;
    }
    pixels = (size_t)image.width * (size_t)image.height;
    rgba_bytes = pixels * 4U;
    readback = (unsigned char *)malloc(rgba_bytes);
    if (readback == NULL) {
        fprintf(stderr, "%s: rgba allocation failed\n", path);
        goto end;
    }
    if (build_weights(&image, 100) != 0) {
        fprintf(stderr, "%s: weight allocation failed\n", path);
        goto end;
    }
    if (make_cpu_reference(&image, allocator) != 0) {
        goto end;
    }
    if (metal_buffers_init(metal, &image, &buffers) != 0) {
        goto end;
    }

    cpu1_ms = bench_cpu_fast4(&image, allocator, 1, iterations);
    cpu2_ms = bench_cpu_fast4(&image, allocator, 2, iterations);
    cpu4_ms = bench_cpu_fast4(&image, allocator, 4, iterations);
    cpu8_ms = bench_cpu_fast4(&image, allocator, 8, iterations);
    metal_indexed_ms = bench_metal_run(metal,
                                       &buffers,
                                       readback,
                                       rgba_bytes,
                                       0,
                                       0,
                                       iterations);
    if (metal_run_once(metal, &buffers, 0) != 0) {
        goto end;
    }
    memcpy(readback, [buffers.result contents], rgba_bytes);
    indexed_ok = compare_rgba(image.cpu_reference,
                              readback,
                              rgba_bytes,
                              &indexed_max_diff);
    metal_rgba_direct_ms = bench_metal_run(metal,
                                           &buffers,
                                           readback,
                                           rgba_bytes,
                                           1,
                                           0,
                                           iterations);
    metal_rgba_direct_readback_ms = bench_metal_run(metal,
                                                    &buffers,
                                                    readback,
                                                    rgba_bytes,
                                                    1,
                                                    1,
                                                    iterations);
    if (metal_run_once(metal, &buffers, 1) != 0) {
        goto end;
    }
    memcpy(readback, [buffers.result contents], rgba_bytes);
    rgba_ok = compare_rgba(image.cpu_reference,
                           readback,
                           rgba_bytes,
                           &rgba_max_diff);
    duplicate_colors = count_palette_duplicate_colors(&image);

    printf("%s,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
           "%.3f,%.3f,%s,%u,%s,%u\n",
           path,
           image.width,
           image.height,
           image.ncolors,
           duplicate_colors,
           decode_raw_ms,
           decode_direct_rgba_ms,
           cpu1_ms,
           cpu2_ms,
           cpu4_ms,
           cpu8_ms,
           metal_indexed_ms,
           metal_rgba_direct_ms,
           metal_rgba_direct_readback_ms,
           indexed_ok == 0 ? "ok" : "diff",
           indexed_max_diff,
           rgba_ok == 0 ? "ok" : "diff",
           rgba_max_diff);
    status = 0;

end:
    if (readback != NULL) {
        free(readback);
    }
    image_free(&image, allocator);
    return status;
}

static void
usage(char const *argv0)
{
    fprintf(stderr,
            "usage: %s [--iterations N] input.six...\n",
            argv0);
}

int
main(int argc, char **argv)
{
    bench_metal_t metal;
    sixel_allocator_t *allocator;
    int iterations;
    int argi;
    int status;

    iterations = 20;
    argi = 1;
    allocator = NULL;
    if (argc > 2 && strcmp(argv[argi], "--iterations") == 0) {
        iterations = atoi(argv[argi + 1]);
        argi += 2;
    }
    if (iterations < 1 || argi >= argc) {
        usage(argv[0]);
        return 2;
    }

    @autoreleasepool {
        if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)
                != SIXEL_OK) {
            fprintf(stderr, "sixel_allocator_new failed\n");
            return 1;
        }
        if (metal_init(&metal) != 0) {
            sixel_allocator_unref(allocator);
            return 1;
        }

        printf("path,width,height,ncolors,palette_duplicate_colors,"
               "decode_raw_ms,decode_direct_rgba_ms,"
               "cpu_fast4_t1_ms,cpu_fast4_t2_ms,cpu_fast4_t4_ms,"
               "cpu_fast4_t8_ms,metal_indexed_ms,metal_rgba_direct_ms,"
               "metal_rgba_direct_readback_ms,metal_indexed_match,"
               "metal_indexed_maxdiff,metal_rgba_direct_match,"
               "metal_rgba_direct_maxdiff\n");
        status = 0;
        for (; argi < argc; ++argi) {
            if (bench_path(argv[argi], &metal, allocator, iterations) != 0) {
                status = 1;
            }
        }
        sixel_allocator_unref(allocator);
    }
    return status;
}
