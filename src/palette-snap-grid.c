/* SPDX-License-Identifier: MIT */
/* P3 targets model the decoder's RGB101 -> RGB8 -> device RGB contract.
 * Cache strategies change computation only, never the target definition. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include <string.h>
#include "allocator.h"
#include "cms.h"
#include "colorspace.h"
#include "loader-common.h"
#include "palette-snap-grid.h"

#define SIXEL_SNAP_GRID_COUNT 1030301U
#define SIXEL_SNAP_GRID_SLOTS 8192U

typedef struct sixel_snap_grid_cell {
    unsigned int key;
    double value[3];
} sixel_snap_grid_cell_t;

typedef struct sixel_snap_grid_vtbl {
    int (*target)(sixel_snap_grid_t *, unsigned int, double *);
} sixel_snap_grid_vtbl_t;

struct sixel_snap_grid {
    sixel_snap_grid_vtbl_t const *vtbl;
    sixel_allocator_t *allocator;
    sixel_color_xform_t *to_linear;
    int colorspace;
    int policy;
    double tones[101];
    double levels[101];
    double contribution[3][101][3];
    double forward[3][3];
    sixel_snap_grid_cell_t *cache;
    double *full;
    unsigned long queries;
    unsigned long hits;
    unsigned long conversions;
};

static int
sixel_snap_grid_direct(sixel_snap_grid_t *grid, unsigned int code,
                        double *target)
{
    double rgb[3];
    double linear[3];

    rgb[0] = grid->tones[code / 10201U];
    rgb[1] = grid->tones[(code / 101U) % 101U];
    rgb[2] = grid->tones[code % 101U];
    ++grid->conversions;
    return sixel_cxform_apply(grid->to_linear, rgb, linear) &&
        sixel_color_convert(target, linear, SIXEL_COLORSPACE_LINEAR,
                            grid->colorspace);
}

static int
sixel_snap_grid_table(sixel_snap_grid_t *grid, unsigned int code,
                       double *target)
{
    double linear[3];
    unsigned int r;
    unsigned int g;
    unsigned int b;
    int channel;

    r = code / 10201U;
    g = (code / 101U) % 101U;
    b = code % 101U;
    for (channel = 0; channel < 3; ++channel) {
        linear[channel] = grid->contribution[0][r][channel]
            + grid->contribution[1][g][channel]
            + grid->contribution[2][b][channel];
    }
    ++grid->conversions;
    return sixel_color_convert(target, linear, SIXEL_COLORSPACE_LINEAR,
                               grid->colorspace);
}

static int
sixel_snap_grid_cached(sixel_snap_grid_t *grid, unsigned int code,
                        double *target)
{
    sixel_snap_grid_cell_t *cell;

    cell = &grid->cache[(code * 2654435761U) % SIXEL_SNAP_GRID_SLOTS];
    if (cell->key == code + 1U) {
        ++grid->hits;
        memcpy(target, cell->value, sizeof(cell->value));
        return 1;
    }
    if (!sixel_snap_grid_table(grid, code, target)) {
        return 0;
    }
    memcpy(cell->value, target, sizeof(cell->value));
    cell->key = code + 1U;
    return 1;
}

static int
sixel_snap_grid_full(sixel_snap_grid_t *grid, unsigned int code,
                      double *target)
{
    ++grid->hits;
    memcpy(target, grid->full + (size_t)code * 3U, 3U * sizeof(double));
    return 1;
}

static sixel_snap_grid_vtbl_t const sixel_snap_grid_methods[] = {
    { sixel_snap_grid_table },
    { sixel_snap_grid_direct },
    { sixel_snap_grid_table },
    { sixel_snap_grid_cached },
    { sixel_snap_grid_full }
};

void
sixel_snap_grid_destroy(sixel_snap_grid_t *grid)
{
    sixel_allocator_t *allocator;

    if (grid == NULL) {
        return;
    }
    sixel_trace_topic_message("palette_contract",
        "LSXSG1|strategy=%d|queries=%lu|hits=%lu|conversions=%lu",
        grid->policy, grid->queries, grid->hits, grid->conversions);
    allocator = grid->allocator;
    sixel_cxform_free(grid->to_linear);
    sixel_allocator_free(allocator, grid->cache);
    sixel_allocator_free(allocator, grid->full);
    sixel_allocator_free(allocator, grid);
    sixel_allocator_unref(allocator);
}

SIXELSTATUS
sixel_snap_grid_new(sixel_snap_grid_t **result, int policy, int colorspace,
                     sixel_allocator_t *allocator)
{
    static char const *const policies[] = {
        "auto", "direct", "table", "cache", "full"
    };
    static double const srgb_xy[8] = {
        .3127, .3290, .64, .33, .30, .60, .15, .06
    };
    static double const p3_xy[8] = {
        .3127, .3290, .68, .32, .265, .69, .15, .06
    };
    static double const unity[3] = {1.0, 1.0, 1.0};
    sixel_snap_grid_t *grid;
    sixel_color_def_t *srgb;
    sixel_color_def_t *p3;
    sixel_color_def_t *p3_linear;
    sixel_color_xform_t *forward;
    SIXELSTATUS status;
    double rgb[3];
    double linear[3];
    unsigned int code;
    int axis;
    int channel;
    int q;

    if (result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;
    if (allocator == NULL || policy < 0 || policy > SIXEL_SNAP_GRID_FULL ||
            (colorspace != SIXEL_COLORSPACE_GAMMA &&
             colorspace != SIXEL_COLORSPACE_LINEAR &&
             colorspace != SIXEL_COLORSPACE_OKLAB &&
             colorspace != SIXEL_COLORSPACE_CIELAB &&
             colorspace != SIXEL_COLORSPACE_DIN99D)) {
        return SIXEL_BAD_ARGUMENT;
    }
    /* Palette-sized cold queries favor the small contribution table.
     * Reuse-heavy experiments can request CACHE/FULL explicitly rather
     * than making every ordinary palette pay their storage/setup cost. */
    if (policy == SIXEL_SNAP_GRID_AUTO) {
        policy = SIXEL_SNAP_GRID_TABLE;
    }
    grid = sixel_allocator_calloc(allocator, 1U, sizeof(*grid));
    if (grid == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    grid->allocator = allocator;
    sixel_allocator_ref(allocator);
    grid->policy = policy;
    grid->colorspace = colorspace;
    grid->vtbl = &sixel_snap_grid_methods[policy];
    srgb = sixel_cdef_rgb(srgb_xy, SIXEL_COLOR_TRC_POWER, unity);
    p3 = sixel_cdef_name("display-p3");
    p3_linear = sixel_cdef_rgb(p3_xy, SIXEL_COLOR_TRC_POWER, unity);
    forward = sixel_cxform_new(srgb, p3_linear, 1);
    grid->to_linear = sixel_cxform_new(p3, srgb, 1);
    sixel_cdef_free(p3_linear);
    sixel_cdef_free(p3);
    sixel_cdef_free(srgb);
    status = SIXEL_BAD_ALLOCATION;
    if (forward == NULL || grid->to_linear == NULL) {
        goto end;
    }
    status = SIXEL_BAD_INPUT;
    for (axis = 0; axis < 3; ++axis) {
        memset(rgb, 0, sizeof(rgb));
        rgb[axis] = 1.0;
        if (!sixel_cxform_apply(forward, rgb, linear)) {
            goto end;
        }
        for (channel = 0; channel < 3; ++channel) {
            grid->forward[channel][axis] = linear[channel];
        }
    }
    for (q = 0; q <= 100; ++q) {
        grid->tones[q] = ((q * 255 + 50) / 100) / 255.0;
        grid->levels[q] = grid->tones[q] <= .04045
            ? grid->tones[q] / 12.92
            : pow((grid->tones[q] + .055) / 1.055, 2.4);
        if (policy == SIXEL_SNAP_GRID_DIRECT) {
            continue;
        }
        for (axis = 0; axis < 3; ++axis) {
            memset(rgb, 0, sizeof(rgb));
            rgb[axis] = grid->tones[q];
            if (!sixel_cxform_apply(grid->to_linear, rgb,
                    grid->contribution[axis][q])) {
                goto end;
            }
        }
    }
    if (policy == SIXEL_SNAP_GRID_CACHE) {
        grid->cache = sixel_allocator_calloc(allocator,
            SIXEL_SNAP_GRID_SLOTS, sizeof(*grid->cache));
        if (grid->cache == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }
    if (policy == SIXEL_SNAP_GRID_FULL) {
        grid->full = sixel_allocator_malloc(allocator,
            (size_t)SIXEL_SNAP_GRID_COUNT * 3U * sizeof(double));
        if (grid->full == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (code = 0U; code < SIXEL_SNAP_GRID_COUNT; ++code) {
            if (!sixel_snap_grid_table(grid, code,
                    grid->full + (size_t)code * 3U)) {
                goto end;
            }
        }
    }
    sixel_trace_topic_message("palette_contract", "LSXSNP1|grid=%s",
        policies[policy]);
    *result = grid;
    grid = NULL;
    status = SIXEL_OK;
end:
    sixel_cxform_free(forward);
    sixel_snap_grid_destroy(grid);
    return status;
}

SIXELSTATUS
sixel_snap_grid_target(sixel_snap_grid_t *grid, unsigned int code,
                        double result[3])
{
    if (grid == NULL || result == NULL || code >= SIXEL_SNAP_GRID_COUNT) {
        return SIXEL_BAD_ARGUMENT;
    }
    ++grid->queries;
    return grid->vtbl->target(grid, code, result)
        ? SIXEL_OK : SIXEL_BAD_INPUT;
}

SIXELSTATUS
sixel_snap_grid_project(sixel_snap_grid_t *grid, double const source[3],
                         int nearest, double lightness_weight,
                         double result[3])
{
    double linear[3];
    double device[3];
    double candidate[3];
    double best[3];
    double weights[3];
    double encoded;
    double distance;
    double delta;
    unsigned int code;
    int lower[3];
    int upper[3];
    int row;
    int column;
    int lo;
    int hi;
    int mid;
    int byte;
    int r;
    int g;
    int b;
    int found;
    SIXELSTATUS status;

    if (grid == NULL || source == NULL || result == NULL ||
            !isfinite(lightness_weight) || lightness_weight < 0.0 ||
            lightness_weight > 1.0 ||
            !sixel_color_convert(linear, source, grid->colorspace,
                                  SIXEL_COLORSPACE_LINEAR)) {
        return SIXEL_BAD_INPUT;
    }
    weights[0] = weights[1] = weights[2] = 1.0;
    if (grid->colorspace == SIXEL_COLORSPACE_OKLAB ||
            grid->colorspace == SIXEL_COLORSPACE_CIELAB ||
            grid->colorspace == SIXEL_COLORSPACE_DIN99D) {
        weights[0] = lightness_weight;
        weights[1] = weights[2] = (1.0 - lightness_weight) * .5;
    }
    for (row = 0; row < 3; ++row) {
        device[row] = 0.0;
        for (column = 0; column < 3; ++column) {
            device[row] += grid->forward[row][column] * linear[column];
        }
        if (!isfinite(device[row])) {
            return SIXEL_BAD_INPUT;
        }
        if (!nearest) {
            encoded = device[row] <= .0031308 ? 12.92 * device[row]
                : 1.055 * pow(device[row], 1.0 / 2.4) - .055;
            encoded = encoded < 0.0 ? 0.0 : encoded > 1.0 ? 1.0 : encoded;
            byte = (int)(encoded * 255.0 + .5);
            lower[row] = upper[row] = (byte * 100 + 127) / 255;
            continue;
        }
        /* Search linear levels: monotonic TRC makes this equivalent to
         * encoded bracketing, without a power evaluation for each query. */
        lo = 0;
        hi = 100;
        while (lo < hi) {
            mid = (lo + hi) / 2;
            if (grid->levels[mid] < device[row]) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        upper[row] = lo;
        lower[row] = lo > 0 && device[row] < grid->levels[lo]
            ? lo - 1 : lo;
    }
    found = 0;
    memset(best, 0, sizeof(best));
    for (r = lower[0]; r <= upper[0]; ++r) {
        for (g = lower[1]; g <= upper[1]; ++g) {
            for (b = lower[2]; b <= upper[2]; ++b) {
                code = (unsigned int)(r * 10201 + g * 101 + b);
                status = sixel_snap_grid_target(grid, code, candidate);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                distance = 0.0;
                for (row = 0; row < 3; ++row) {
                    delta = candidate[row] - best[row];
                    distance += weights[row] * delta *
                        (candidate[row] + best[row] - 2.0 * source[row]);
                }
                if (!isfinite(distance)) {
                    return SIXEL_BAD_INPUT;
                }
                if (!found || distance < 0.0) {
                    memcpy(best, candidate, sizeof(best));
                    found = 1;
                }
            }
        }
    }
    memcpy(result, best, sizeof(best));
    return SIXEL_OK;
}
