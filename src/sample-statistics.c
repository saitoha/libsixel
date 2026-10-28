/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
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
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "allocator.h"
#include "sample-statistics.h"
#include "colorspace.h"
#include "palette-common-snap.h"
#include "pixelformat.h"

/* Count output-distinguishable triplets while the producer visits samples.
 * Perceptual coordinates must be converted together: an L/a/b span is not
 * an RGB safe-tone span. The marginal observations remain unrounded. */
static SIXELSTATUS
sixel_sample_safe_color_add(sixel_sample_statistics_t *statistics,
    double const coordinates[3], double const gamma_source[3])
{
    double native[3];
    double gamma[3];
    double projected[3];
    double bounded;
    float *storage;
    uint32_t *safe_keys;
    size_t capacity;
    uint32_t key;
    uint32_t hash;
    unsigned int axis;
    unsigned int byte;
    unsigned int slot;
    int format;
    int cached;

    if (!statistics->safe_colors_valid) {
        return SIXEL_OK;
    }
    if (statistics->byte_coordinates &&
            statistics->safe_color_count == SIXEL_SAMPLE_SAFE_LIMIT) {
        return SIXEL_OK;
    }
    format = SIXEL_PIXELFORMAT_RGBFLOAT32;
    switch (statistics->colorspace) {
    case SIXEL_COLORSPACE_OKLAB:
        format = SIXEL_PIXELFORMAT_OKLABFLOAT32;
        break;
    case SIXEL_COLORSPACE_CIELAB:
        format = SIXEL_PIXELFORMAT_CIELABFLOAT32;
        break;
    case SIXEL_COLORSPACE_DIN99D:
        format = SIXEL_PIXELFORMAT_DIN99DFLOAT32;
        break;
    default:
        break;
    }
    key = 0u;
    if (statistics->byte_coordinates &&
            statistics->colorspace == SIXEL_COLORSPACE_GAMMA) {
        for (axis = 0u; axis < 3u; ++axis) {
            byte = (unsigned int)coordinates[axis];
            key = (key << 8u) | sixel_safe_tones[byte];
            gamma[axis] = sixel_safe_tones[byte] / 255.0;
        }
    } else {
        for (axis = 0u; axis < 3u; ++axis) {
            native[axis] = statistics->byte_coordinates
                ? sixel_pixelformat_byte_to_float(format, (int)axis,
                    (unsigned char)coordinates[axis]) : coordinates[axis];
        }
        if (gamma_source != NULL) {
            memcpy(gamma, gamma_source, sizeof(gamma));
        } else if (!sixel_color_convert(gamma, native, statistics->colorspace,
                SIXEL_COLORSPACE_GAMMA)) {
            /* Optional recovery evidence must not reject extended input
             * accepted by the solver. An unprojectable population simply
             * has no usable safe-tone evidence for automatic refinement. */
            statistics->safe_colors_valid = 0;
            statistics->safe_color_count = 0u;
            return SIXEL_OK;
        }
        for (axis = 0u; axis < 3u; ++axis) {
            bounded = gamma[axis] < 0.0 ? 0.0 : gamma[axis];
            bounded = bounded > 1.0 ? 1.0 : bounded;
            byte = (unsigned int)(bounded * 255.0 + 0.5);
            key = (key << 8u) | sixel_safe_tones[byte];
            gamma[axis] = sixel_safe_tones[byte] / 255.0;
        }
    }
    /* Zero denotes an empty slot; all RGB keys fit in 24 bits. */
    ++key;
    hash = key ^ (key >> 16u);
    hash = (uint32_t)((uint64_t)hash * 0x7feb352du);
    hash ^= hash >> 15u;
    slot = hash & (SIXEL_SAMPLE_SAFE_LIMIT * 2u - 1u);
    cached = 0;
    while (statistics->safe_color_keys[slot] != 0u) {
        if (statistics->safe_color_keys[slot] == key) {
            cached = 1;
            break;
        }
        slot = (slot + 1u) & (SIXEL_SAMPLE_SAFE_LIMIT * 2u - 1u);
    }
    if (!statistics->byte_coordinates) {
        /* Only the colorspace producer projects output tones back into
         * native coordinates. Binning consumes this aligned evidence and
         * never performs a color conversion or reads source image pixels.
         * Repeated wire colors reuse exactly the same projected triplet. */
        if (cached) {
            for (axis = 0u; axis < 3u; ++axis) {
                projected[axis] = statistics->safe_color_values[slot][axis];
            }
        } else if (!sixel_color_convert(projected, gamma,
                SIXEL_COLORSPACE_GAMMA, statistics->colorspace)) {
            statistics->safe_colors_valid = 0;
            statistics->safe_color_count = 0u;
            return SIXEL_OK;
        }
        if (statistics->point_count == statistics->safe_capacity) {
            capacity = statistics->safe_capacity == 0u
                ? 256u : statistics->safe_capacity * 2u;
            if (capacity < statistics->safe_capacity ||
                    capacity > SIZE_MAX / (3u * sizeof(float))) {
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
            storage = sixel_allocator_realloc(statistics->allocator,
                statistics->safe_coordinates, capacity * 3u * sizeof(float));
            if (storage == NULL) {
                return SIXEL_BAD_ALLOCATION;
            }
            statistics->safe_coordinates = storage;
            safe_keys = sixel_allocator_realloc(statistics->allocator,
                statistics->safe_keys, capacity * sizeof(*safe_keys));
            if (safe_keys == NULL) {
                return SIXEL_BAD_ALLOCATION;
            }
            statistics->safe_keys = safe_keys;
            statistics->safe_capacity = capacity;
        }
        for (axis = 0u; axis < 3u; ++axis) {
            statistics->safe_coordinates[statistics->point_count * 3u + axis]
                = (float)projected[axis];
            if (!cached &&
                    statistics->safe_color_count < SIXEL_SAMPLE_SAFE_LIMIT) {
                statistics->safe_color_values[slot][axis] =
                    (float)projected[axis];
            }
        }
        statistics->safe_keys[statistics->point_count] = key - 1u;
    }
    if (!cached && statistics->safe_color_count < SIXEL_SAMPLE_SAFE_LIMIT) {
        statistics->safe_color_keys[slot] = key;
        ++statistics->safe_color_count;
    }
    return SIXEL_OK;
}

static size_t
sixel_sample_axis_hash(double value, size_t mask)
{
    uint64_t bits;
    uint32_t hash;

    /* Canonicalize both signs of zero, which denote the same coordinate. */
    if (value == 0.0) {
        value = 0.0;
    }
    memcpy(&bits, &value, sizeof(bits));
    hash = (uint32_t)bits ^ (uint32_t)(bits >> 32);
    hash ^= hash >> 16;
    hash = (uint32_t)((uint64_t)hash * 0x7feb352dU);
    hash ^= hash >> 15;
    hash = (uint32_t)((uint64_t)hash * 0x846ca68bU);
    hash ^= hash >> 16;
    return (size_t)hash & mask;

}

static SIXELSTATUS
sixel_sample_axis_add(sixel_sample_statistics_t *statistics,
    unsigned int axis, double value)
{
    sixel_sample_axis_entry_t *entries;
    sixel_sample_axis_entry_t *old;
    size_t capacity;
    size_t slot;
    size_t index;

    capacity = statistics->capacities[axis];
    if (capacity != 0u) {
        slot = sixel_sample_axis_hash(value, capacity - 1u);
        while (statistics->axes[axis][slot].count != 0u) {
            if (statistics->axes[axis][slot].value == value) {
                ++statistics->axes[axis][slot].count;
                return SIXEL_OK;
            }
            slot = (slot + 1u) & (capacity - 1u);
        }
    }
    if (capacity == 0u || statistics->sizes[axis] >= capacity / 2u) {
        if (capacity > SIZE_MAX / (2u * sizeof(*entries))) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        capacity = capacity == 0u ? 64u : capacity * 2u;
        entries = sixel_allocator_calloc(statistics->allocator,
            capacity, sizeof(*entries));
        if (entries == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        old = statistics->axes[axis];
        for (index = 0u; index < statistics->capacities[axis]; ++index) {
            if (old[index].count == 0u) {
                continue;
            }
            slot = sixel_sample_axis_hash(old[index].value, capacity - 1u);
            while (entries[slot].count != 0u) {
                slot = (slot + 1u) & (capacity - 1u);
            }
            entries[slot] = old[index];
        }
        sixel_allocator_free(statistics->allocator, old);
        statistics->axes[axis] = entries;
        statistics->capacities[axis] = capacity;
    }
    slot = sixel_sample_axis_hash(value, capacity - 1u);
    while (statistics->axes[axis][slot].count != 0u) {
        slot = (slot + 1u) & (capacity - 1u);
    }
    statistics->axes[axis][slot].value = value;
    statistics->axes[axis][slot].count = 1u;
    ++statistics->sizes[axis];
    return SIXEL_OK;
}

SIXELSTATUS
sixel_sample_statistics_new(sixel_sample_statistics_t **out,
    int colorspace, int byte_coordinates, sixel_allocator_t *allocator)
{
    sixel_sample_statistics_t *statistics;

    if (out == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *out = NULL;
    statistics = sixel_allocator_calloc(allocator, 1u, sizeof(*statistics));
    if (statistics == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    statistics->allocator = allocator;
    sixel_allocator_ref(allocator);
    statistics->references = 1u;
    statistics->colorspace = colorspace;
    statistics->byte_coordinates = byte_coordinates != 0;
    statistics->safe_colors_valid = 1;
    *out = statistics;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_sample_statistics_observe_gamma(sixel_sample_statistics_t *statistics,
    double const coordinates[3], double const gamma_source[3])
{
    SIXELSTATUS status;
    unsigned int axis;
    double value;

    if (statistics == NULL || coordinates == NULL ||
            statistics->point_count == SIZE_MAX) {
        return SIXEL_BAD_ARGUMENT;
    }
    /* Validate the complete observation before modifying any marginal. */
    for (axis = 0u; axis < 3u; ++axis) {
        value = coordinates[axis];
        if (!isfinite(value) || (statistics->byte_coordinates &&
                (value < 0.0 || value > 255.0 || floor(value) != value))) {
            return SIXEL_BAD_INPUT;
        }
        if (gamma_source != NULL && !isfinite(gamma_source[axis])) {
            return SIXEL_BAD_INPUT;
        }
    }
    for (axis = 0u; axis < 3u; ++axis) {
        value = coordinates[axis];
        if (statistics->byte_coordinates) {
            ++statistics->byte_counts[axis][(unsigned int)value];
        } else {
            status = sixel_sample_axis_add(statistics, axis, value);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }
        if (statistics->point_count == 0u ||
                value < statistics->minimum[axis]) {
            statistics->minimum[axis] = value;
        }
        if (statistics->point_count == 0u ||
                value > statistics->maximum[axis]) {
            statistics->maximum[axis] = value;
        }
    }
    status = sixel_sample_safe_color_add(statistics, coordinates, gamma_source);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    ++statistics->point_count;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_sample_statistics_observe(sixel_sample_statistics_t *statistics,
    double const coordinates[3])
{
    return sixel_sample_statistics_observe_gamma(statistics, coordinates, NULL);
}

void
sixel_sample_statistics_ref(sixel_sample_statistics_t *statistics)
{
    if (statistics != NULL) {
        ++statistics->references;
    }
}

void
sixel_sample_statistics_unref(sixel_sample_statistics_t *statistics)
{
    sixel_allocator_t *allocator;
    unsigned int axis;

    if (statistics == NULL || --statistics->references != 0u) {
        return;
    }
    allocator = statistics->allocator;
    for (axis = 0u; axis < 3u; ++axis) {
        sixel_allocator_free(allocator, statistics->axes[axis]);
    }
    sixel_allocator_free(allocator, statistics->safe_coordinates);
    sixel_allocator_free(allocator, statistics->safe_keys);
    sixel_allocator_free(allocator, statistics);
    sixel_allocator_unref(allocator);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
