/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "progressive-apply.h"
#include "dither-policy-bluenoise.h"

/* Keep packing, device work and acceptance bounded so the owner can check
 * cancellation, deadlines and ordinary-operation priority between batches. */
#define REPAIR_APPLICATION_BATCH 256

static SIXELSTATUS
sixel_repair_application_query(sixel_repair_application_t const *application,
    size_t position, float query[3])
{
    float const *source;
    double value;
    int x;
    int y;
    int channel;

    source = application->source + position * 3;
    x = (int)(position % application->width) + application->origin_x;
    y = (int)(position / application->width) + application->origin_y;
    for (channel = 0; channel < 3; ++channel) {
        value = source[channel];
        if (position != application->protected_position) {
            value += application->strength * (32.0 / 255.0) *
                sixel_bluenoise_sample(x, y, application->phase_x,
                    application->phase_y, application->channel_rgb, channel);
        }
        if (!isfinite(value) || fabs(value) > FLT_MAX) return SIXEL_BAD_INPUT;
        query[channel] = (float)value;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_repair_application_accept(sixel_repair_application_t *application,
    size_t position, unsigned int winner, sixel_repair_work_t *work)
{
    float const *color;
    double distance;

    /* Noise and GPU floats choose a candidate, but only this double check
     * against the unperturbed target may authorize paint. Rejection never
     * retries an unperturbed nearest color or changes retained history. */
    color = application->colors[application->palette[winner]].working;
    distance = sixel_repair_distance2(application->source + position * 3,
        color);
    ++work->distance_evaluations;
    if (!isfinite(distance) || !isfinite(application->errors[position])) {
        return SIXEL_BAD_INPUT;
    }
    if (sixel_repair_improves(application->errors[position], distance)) {
        application->paint[position] = 1;
        application->indices[position] = (unsigned char)winner;
        application->gains[position] = application->errors[position] -
            distance;
        ++application->painted;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_repair_application_lookup(sixel_repair_application_t *application,
    float const *queries, size_t count, unsigned int first,
    unsigned int colors, sixel_repair_gpu_match_t *matches)
{
    float palette[255 * 3];
    sixel_repair_gpu_lookup_t request;
    unsigned int i;
    SIXELSTATUS status;

    for (i = 0; i < colors; ++i) {
        memcpy(palette + i * 3, application->colors[
            application->palette[first + i]].working, 3 * sizeof(float));
    }
    request.queries = queries;
    request.palette = palette;
    request.count = count;
    request.colors = colors;
    request.policy = application->gpu_policy;
    status = sixel_repair_gpu_lookup(application->gpu, &request, matches);
    if (status == SIXEL_FALSE) application->gpu_policy = SIXEL_GPU_POLICY_OFF;
    if (status == SIXEL_OK) application->gpu_used = 1;
    return status;
}

static SIXELSTATUS
sixel_repair_application_batch(sixel_repair_application_t *application,
    sixel_repair_work_t *work)
{
    float queries[REPAIR_APPLICATION_BATCH * 3];
    size_t positions[REPAIR_APPLICATION_BATCH];
    sixel_repair_gpu_match_t matches[REPAIR_APPLICATION_BATCH];
    size_t count;
    size_t end;
    size_t visits;
    size_t limit;
    size_t i;
    SIXELSTATUS status;

    count = 0;
    end = application->position;
    visits = work->pixel_limit - work->pixel_visits;
    if (visits > REPAIR_APPLICATION_BATCH) visits = REPAIR_APPLICATION_BATCH;
    limit = (work->distance_limit - work->distance_evaluations) /
        (application->color_count + 1);
    if (limit == 0) return SIXEL_FALSE;
    while (end < application->count &&
            end - application->position < visits && count < limit) {
        if (application->selected[end]) {
            status = sixel_repair_application_query(application, end,
                queries + count * 3);
            if (status != SIXEL_OK) return status;
            positions[count++] = end;
        }
        ++end;
    }
    visits = end - application->position;
    work->pixel_visits += visits;
    work->distance_evaluations += count * application->color_count;
    if (count != 0) {
        status = sixel_repair_application_lookup(application, queries,
            count, 0, application->color_count, matches);
        if (status != SIXEL_OK) {
            /* Charge an attempted batch conservatively even if the device
             * failed before finishing. CPU fallback resumes this cursor at
             * the next checkpoint and must not spend the same budget twice. */
            return status == SIXEL_FALSE ? SIXEL_OK : status;
        }
    }
    /* No speculative output or cursor is consumed until lookup succeeds. */
    memset(application->paint + application->position, 0, visits);
    memset(application->indices + application->position, 0, visits);
    memset(application->gains + application->position, 0,
        visits * sizeof(*application->gains));
    for (i = 0; i < count; ++i) {
        status = sixel_repair_application_accept(application, positions[i],
            matches[i].index, work);
        if (status != SIXEL_OK) return status;
    }
    application->position = end;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_repair_application_begin(sixel_repair_application_t *application)
{
    size_t height;

    if (application == NULL || application->count == 0 ||
            application->width <= 0 || application->source == NULL ||
            application->errors == NULL || application->selected == NULL ||
            application->colors == NULL || application->palette == NULL ||
            application->indices == NULL || application->paint == NULL ||
            application->gains == NULL || application->color_count > 255 ||
            application->origin_x < 0 || application->origin_y < 0 ||
            !isfinite(application->strength) || application->strength < 0 ||
            application->phase_x >= 64 || application->phase_y >= 64 ||
            application->count % (size_t)application->width != 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    height = application->count / (size_t)application->width;
    if (height > INT_MAX || application->origin_x >
            INT_MAX - application->width || application->origin_y >
            INT_MAX - (int)height) {
        return SIXEL_BAD_ARGUMENT;
    }
    application->position = 0;
    application->candidate = 0;
    application->started = 0;
    application->painted = 0;
    application->gpu_used = 0;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_repair_application_advance(sixel_repair_application_t *application,
    sixel_repair_work_t *work, int *done)
{
    size_t position;
    float const *color;
    double distance;
    unsigned int chunk;
    unsigned int colors;
    sixel_repair_gpu_match_t match;
    SIXELSTATUS status;

    if (application == NULL || work == NULL || done == NULL ||
            work->pixel_limit == 0 || work->distance_limit == 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    *done = 0;
    if (!application->started && application->gpu != NULL &&
            application->gpu_policy != SIXEL_GPU_POLICY_OFF &&
            application->color_count != 0 &&
            work->pixel_visits < work->pixel_limit &&
            work->distance_evaluations < work->distance_limit) {
        status = sixel_repair_application_batch(application, work);
        if (status != SIXEL_FALSE) {
            *done = application->position == application->count;
            return status;
        }
    }
    for (chunk = 0; chunk < 256; ++chunk) {
        if (application->position == application->count) {
            *done = 1;
            return SIXEL_OK;
        }
        if (work->distance_evaluations >= work->distance_limit) {
            return SIXEL_OK;
        }
        position = application->position;
        if (!application->started) {
            if (work->pixel_visits >= work->pixel_limit) {
                return SIXEL_OK;
            }
            ++work->pixel_visits;
            application->paint[position] = 0;
            application->indices[position] = 0;
            application->gains[position] = 0;
            if (!application->selected[position] ||
                    application->color_count == 0) {
                ++application->position;
                continue;
            }
            status = sixel_repair_application_query(application, position,
                application->query);
            if (status != SIXEL_OK) return status;
            application->candidate = 0;
            application->winner = 0;
            application->nearest = DBL_MAX;
            application->started = 1;
        }
        if (application->candidate < application->color_count) {
            if (application->gpu != NULL &&
                    application->gpu_policy != SIXEL_GPU_POLICY_OFF) {
                /* A tiny budget can split even one pixel's palette search.
                 * Preserve first-slot ties across slices and yield before
                 * another submission, including under forced GPU policy. */
                colors = application->color_count - application->candidate;
                if (colors > work->distance_limit -
                        work->distance_evaluations) {
                    colors = (unsigned int)(work->distance_limit -
                        work->distance_evaluations);
                }
                status = sixel_repair_application_lookup(application,
                    application->query, 1, application->candidate, colors,
                    &match);
                work->distance_evaluations += colors;
                if (status == SIXEL_OK) {
                    if (match.distance < application->nearest) {
                        application->nearest = match.distance;
                        application->winner = application->candidate +
                            match.index;
                    }
                    application->candidate += colors;
                    return SIXEL_OK;
                }
                return status == SIXEL_FALSE ? SIXEL_OK : status;
            }
            color = application->colors[
                application->palette[application->candidate]].working;
            distance = sixel_repair_distance2(application->query, color);
            ++work->distance_evaluations;
            if (!isfinite(distance)) {
                return SIXEL_BAD_INPUT;
            }
            if (distance < application->nearest) {
                application->nearest = distance;
                application->winner = application->candidate;
            }
            ++application->candidate;
            continue;
        }
        status = sixel_repair_application_accept(application, position,
            application->winner, work);
        if (status != SIXEL_OK) return status;
        ++application->position;
        application->started = 0;
    }
    *done = application->position == application->count;
    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
