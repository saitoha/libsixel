/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <string.h>
#include "progressive.h"
#include "colorspace.h"

/* Match the assessment signal: encoded luma in gamma, physical linear Y
 * otherwise. An opponent-space L component is not interchangeable with Y. */
int
sixel_repair_video_luma(float const color[3], int workspace, double *luma)
{
    double input[3];
    double rgb[3];
    int channel;
    int basis;

    basis = workspace == SIXEL_COLORSPACE_GAMMA
        ? SIXEL_COLORSPACE_GAMMA : SIXEL_COLORSPACE_LINEAR;
    if (workspace == basis) {
        *luma = .2126 * color[0] + .7152 * color[1] + .0722 * color[2];
        return isfinite(*luma);
    }
    for (channel = 0; channel < 3; ++channel) {
        input[channel] = color[channel];
    }
    if (!sixel_color_convert(rgb, input, workspace, basis)) {
        return 0;
    }
    *luma = .2126 * rgb[0] + .7152 * rgb[1] + .0722 * rgb[2];
    return isfinite(*luma);
}

SIXELSTATUS
sixel_repair_video_enable_owned(sixel_repair_context_t *context,
    unsigned char const *paint, size_t count)
{
    if (context == NULL || paint == NULL || count != context->count ||
            context->audit_started || context->selection != NULL ||
            context->video_excluded != NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    context->video_excluded = sixel_allocator_malloc(context->allocator,
                                                    count);
    context->video_error = sixel_allocator_calloc(context->allocator,
        count, sizeof(double));
    context->video_luma = sixel_allocator_calloc(context->allocator,
        count, sizeof(double));
    context->video_after = sixel_allocator_calloc(context->allocator,
        count, sizeof(double));
    context->video_priority = sixel_allocator_calloc(context->allocator,
        count, sizeof(double));
    if (context->video_age == NULL) {
        context->video_age = sixel_allocator_calloc(context->allocator,
                                                     count, 1);
    }
    if (context->video_excluded == NULL || context->video_error == NULL ||
            context->video_luma == NULL || context->video_after == NULL ||
            context->video_priority == NULL ||
            context->video_age == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(context->video_excluded, paint, count);
    context->video_active = 1;
    return SIXEL_OK;
}

/* This inexpensive loss measures local mean error and false/missing
 * gradients. It is a spatial admission guard, not an MS-SSIM guarantee.
 * Source-alpha holes have no known target and do not form filter edges. */
static double
sixel_video_loss(sixel_repair_context_t const *context,
    double const *errors, size_t position)
{
    size_t neighbors[4];
    size_t x;
    size_t y;
    size_t count;
    size_t i;
    double mean;
    double gradient;
    double difference;
    double samples;
    double target_mean;
    double target_square;
    double target;
    double variance;

    x = position % context->width;
    y = position / context->width;
    count = 0;
    if (x) neighbors[count++] = position - 1;
    if (x + 1 < (size_t)context->width) neighbors[count++] = position + 1;
    if (y) neighbors[count++] = position - context->width;
    if (y + 1 < (size_t)context->height) {
        neighbors[count++] = position + context->width;
    }
    mean = errors[position];
    target_mean = context->video_luma[position];
    target_square = target_mean * target_mean;
    gradient = 0;
    samples = 1;
    for (i = 0; i < count; ++i) {
        if (context->domain[neighbors[i]]) {
            mean += errors[neighbors[i]];
            target = context->video_luma[neighbors[i]];
            target_mean += target;
            target_square += target * target;
            difference = errors[position] - errors[neighbors[i]];
            gradient += difference * difference;
            ++samples;
        }
    }
    mean /= samples;
    target_mean /= samples;
    variance = fmax(0, target_square / samples - target_mean * target_mean);
    mean *= mean;
    gradient /= samples;
    /* Normalize against target brightness/contrast with the usual SSIM
     * stabilizers. Bound both terms so one edge beside an excluded ordinary
     * pixel cannot outweigh correction of an entire flat region. This local
     * surrogate does not implement SSIM's windows or pyramid. */
    return mean / (mean + 2 * target_mean * target_mean + .0001) +
        .25 * gradient / (gradient + 2 * variance + .0009) +
        .125 * errors[position] * errors[position];
}

SIXELSTATUS
sixel_repair_video_select(sixel_repair_context_t *context,
    sixel_repair_work_t *work, int *done)
{
    size_t position;
    unsigned int chunk;
    double score;

    *done = 0;
    for (chunk = 0; chunk < 256; ++chunk) {
        if (context->video_stage == 2) {
            *done = 1;
            return SIXEL_OK;
        }
        if (work->pixel_visits >= work->pixel_limit) return SIXEL_OK;
        ++work->pixel_visits;
        position = context->video_cursor++;
        if (context->video_stage == 0) {
            score = 0;
            if (context->domain[position] &&
                    !context->video_excluded[position] &&
                    context->errors[position] > context->stop_squared) {
                score = sixel_video_loss(context, context->video_error,
                    position) * (1 + context->video_age[position] / 16.0);
            }
            if (!isfinite(score)) return SIXEL_BAD_INPUT;
            context->video_priority[position] = score;
            if (score > context->video_maximum) context->video_maximum = score;
        } else {
            /* Keep a bounded structural band, re-estimated for each source.
             * When only chromatic error remains, permit a color-only pass. */
            if (context->video_maximum == 0 &&
                    !context->video_excluded[position]) {
                context->video_priority[position] = 1;
            } else if (context->video_priority[position] <
                    context->video_maximum / 8) {
                context->video_priority[position] = 0;
            }
        }
        if (context->video_cursor == context->count) {
            context->video_cursor = 0;
            ++context->video_stage;
        }
    }
    return SIXEL_OK;
}

SIXELSTATUS
sixel_repair_video_guard(sixel_repair_context_t *context,
    sixel_repair_work_t *work, int *done, int *accepted)
{
    float const *source;
    float const *color;
    size_t position;
    unsigned int chunk;
    double target;
    double candidate;
    double tolerance;

    *done = *accepted = 0;
    source = (float const *)sixel_frame_get_pixels(context->source);
    for (chunk = 0; chunk < 256; ++chunk) {
        if (context->video_guard_stage == 2) {
            tolerance = 64 * DBL_EPSILON *
                fmax(1, context->video_before_loss);
            *accepted = context->video_after_loss <=
                context->video_before_loss + tolerance;
            *done = 1;
            return SIXEL_OK;
        }
        if (work->pixel_visits >= work->pixel_limit) return SIXEL_OK;
        ++work->pixel_visits;
        position = context->video_cursor++;
        if (context->video_guard_stage == 0) {
            context->video_after[position] = context->video_error[position];
            if (context->paint[position]) {
                color = context->application.colors[
                    context->application.palette[
                        context->indices[position]]].working;
                if (!sixel_repair_video_luma(source + position * 3,
                        context->workspace, &target) ||
                        !sixel_repair_video_luma(color, context->workspace,
                            &candidate)) {
                    return SIXEL_BAD_INPUT;
                }
                context->video_after[position] = target - candidate;
            }
        } else if (context->domain[position]) {
            context->video_before_loss += sixel_video_loss(context,
                context->video_error, position);
            context->video_after_loss += sixel_video_loss(context,
                context->video_after, position);
            if (!isfinite(context->video_before_loss) ||
                    !isfinite(context->video_after_loss)) {
                return SIXEL_BAD_INPUT;
            }
        }
        if (context->video_cursor == context->count) {
            context->video_cursor = 0;
            ++context->video_guard_stage;
        }
    }
    return SIXEL_OK;
}
