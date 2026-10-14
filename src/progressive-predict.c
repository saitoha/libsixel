/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "progressive-predict.h"
#include "progressive-palette.h"

void
sixel_repair_predict_reset(sixel_repair_predict_model_t *model, double stop)
{
    double edge;
    int band;

    memset(model, 0, sizeof(*model));
    model->stop_squared = stop;
    model->upper[0] = stop;
    edge = fmax(stop, 1.0 / (255.0 * 255.0));
    for (band = 1; band < SIXEL_REPAIR_PREDICT_BANDS - 1; ++band) {
        edge = edge > DBL_MAX / 4 ? DBL_MAX : edge * 4;
        model->upper[band] = edge;
    }
}

int
sixel_repair_predict_band(sixel_repair_predict_model_t const *model,
    double error)
{
    int band;

    for (band = 0; band < SIXEL_REPAIR_PREDICT_BANDS - 1; ++band) {
        if (error <= model->upper[band]) {
            break;
        }
    }
    return band;
}

static int
sixel_repair_predict_less(sixel_repair_predict_point_t const *a,
    sixel_repair_predict_point_t const *b)
{
    return a->priority < b->priority ||
        (a->priority == b->priority && a->position < b->position);
}

void
sixel_repair_predict_add(sixel_repair_predict_model_t *model,
    size_t position, float const target[3], double error)
{
    sixel_repair_predict_point_t point;
    sixel_repair_predict_band_t *band;
    uint64_t hash;
    size_t slot;
    size_t parent;
    size_t child;

    band = model->bands + sixel_repair_predict_band(model, error);
    ++band->population;
    band->sum += error;
    band->maximum = fmax(band->maximum, error);
    /* SplitMix64's permutation with a fixed seed. This is deterministic
     * sampling, not a probabilistic coverage claim or a quality bound. */
    hash = (uint64_t)position + UINT64_C(0x9e3779b97f4a7c15);
    hash = (hash ^ (hash >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    hash = (hash ^ (hash >> 27)) * UINT64_C(0x94d049bb133111eb);
    point.priority = hash ^ (hash >> 31);
    point.position = position;
    point.error = error;
    memcpy(point.target, target, sizeof(point.target));
    if (band->count < SIXEL_REPAIR_PREDICT_SAMPLES) {
        slot = band->count++;
        while (slot > 0) {
            parent = (slot - 1) / 2;
            if (!sixel_repair_predict_less(band->points + parent, &point)) {
                break;
            }
            band->points[slot] = band->points[parent];
            slot = parent;
        }
    } else {
        if (!sixel_repair_predict_less(&point, band->points)) {
            return;
        }
        slot = 0;
        while ((child = slot * 2 + 1) < band->count) {
            if (child + 1 < band->count && sixel_repair_predict_less(
                    band->points + child, band->points + child + 1)) {
                ++child;
            }
            if (!sixel_repair_predict_less(&point, band->points + child)) {
                break;
            }
            band->points[slot] = band->points[child];
            slot = child;
        }
    }
    band->points[slot] = point;
}

static int
sixel_repair_predict_order(void const *a, void const *b)
{
    sixel_repair_predict_point_t const *left;
    sixel_repair_predict_point_t const *right;

    left = *(sixel_repair_predict_point_t const * const *)a;
    right = *(sixel_repair_predict_point_t const * const *)b;
    return sixel_repair_predict_less(left, right) ? -1
        : sixel_repair_predict_less(right, left);
}

SIXELSTATUS
sixel_repair_predict(sixel_repair_predict_model_t const *model,
    float const (*palette)[3], unsigned int colors, double threshold,
    size_t quota, sixel_repair_prediction_t *prediction)
{
    sixel_repair_predict_point_t const *points[SIXEL_REPAIR_PREDICT_SAMPLES];
    sixel_repair_predict_point_t const *point;
    sixel_repair_predict_band_t const *band;
    size_t count;
    size_t sample;
    size_t quotas[SIXEL_REPAIR_PREDICT_BANDS];
    size_t remaining;
    unsigned int color;
    int index;
    int channel;
    int changed;
    double weight;
    double after;
    double nearest;
    double maximum;

    if (model == NULL || prediction == NULL || colors > 255 ||
            (palette == NULL && colors != 0) || !isfinite(threshold) ||
            threshold < model->stop_squared ||
            quota > SIXEL_REPAIR_PREDICT_SAMPLES) {
        return SIXEL_BAD_ARGUMENT;
    }
    for (color = 0; color < colors; ++color) {
        for (channel = 0; channel < 3; ++channel) {
            if (!isfinite(palette[color][channel])) {
                return SIXEL_BAD_ARGUMENT;
            }
        }
    }
    memset(prediction, 0, sizeof(*prediction));
    prediction->exact = 1;
    for (index = 0; index < SIXEL_REPAIR_PREDICT_BANDS; ++index) {
        quotas[index] = quota;
    }
    /* Bounded water filling gives every nonempty residual band support.
     * This is a matched total-sample-cost experiment, not an optimal
     * variance allocation inferred without evidence. */
    remaining = quota ? 0 : SIXEL_REPAIR_PREDICT_SAMPLES;
    while (remaining) {
        changed = 0;
        for (index = 0; index < SIXEL_REPAIR_PREDICT_BANDS && remaining;
                ++index) {
            if (quotas[index] < model->bands[index].count) {
                ++quotas[index];
                --remaining;
                changed = 1;
            }
        }
        if (!changed) {
            break;
        }
    }
    for (index = 0; index < SIXEL_REPAIR_PREDICT_BANDS; ++index) {
        band = model->bands + index;
        if (band->population == 0) {
            continue;
        }
        for (sample = 0; sample < band->count; ++sample) {
            points[sample] = band->points + sample;
        }
        qsort(points, band->count, sizeof(*points),
            sixel_repair_predict_order);
        count = band->count < quotas[index] ? band->count : quotas[index];
        weight = (double)band->population / count;
        prediction->samples += count;
        prediction->exact &= count == band->population;
        maximum = 0;
        for (sample = 0; sample < count; ++sample) {
            point = points[sample];
            after = point->error;
            if (after > model->stop_squared && after >= threshold) {
                prediction->selected += weight;
                nearest = DBL_MAX;
                for (color = 0; color < colors; ++color) {
                    nearest = fmin(nearest, sixel_repair_distance2(
                        point->target, palette[color]));
                    ++prediction->distances;
                }
                if (colors && sixel_repair_improves(after, nearest)) {
                    after = nearest;
                    prediction->painted += weight;
                    prediction->resolved += weight *
                        (after <= model->stop_squared);
                    prediction->gain += weight * (point->error - after);
                    prediction->excess_gain += weight *
                        (fmax(0, point->error - model->stop_squared) -
                         fmax(0, after - model->stop_squared));
                }
            }
            maximum = fmax(maximum, after);
        }
        prediction->maximum_lower = fmax(prediction->maximum_lower, maximum);
        prediction->maximum_upper = fmax(prediction->maximum_upper,
            count == band->population ? maximum : band->maximum);
    }
    return SIXEL_OK;
}
