/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <inttypes.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/progressive.h"
#include "src/progressive-measure.h"
#include "src/encoder-core.h"
#include "src/sixel_decode_pixels.h"
#include "src/compat_stub.h"
#include "src/timer.h"

typedef struct progressive_study_sink {
    FILE *file;
    size_t bytes;
    int anchor;
} progressive_study_sink_t;

static int
progressive_study_write(char *data, int size, void *priv)
{
    progressive_study_sink_t *sink;
    char anchor[2];

    sink = priv;
    if (sink->anchor) {
        anchor[0] = '\033';
        anchor[1] = (char)sink->anchor;
        if (fwrite(anchor, 1, 2, sink->file) != 2) {
            return -1;
        }
        sink->bytes += 2;
        sink->anchor = 0;
    }
    if (size < 0 || fwrite(data, 1, (size_t)size, sink->file) != (size_t)size) {
        return -1;
    }
    sink->bytes += (size_t)size;
    return 0;
}

static SIXELSTATUS
progressive_study_load(sixel_frame_t *frame, void *priv)
{
    sixel_frame_t **source;

    source = priv;
    if (*source != NULL) {
        return SIXEL_BAD_INPUT;
    }
    sixel_frame_ref(frame);
    *source = frame;
    return SIXEL_OK;
}

/* Decode the entire saved capture with the independent stream compositor.
 * Compare against the prepared immutable target, never encoder acc. This is
 * a canonical -W observer; perceptual evaluation belongs to assessment. */
static int
progressive_study_observe(progressive_study_sink_t *sink,
    sixel_progressive_t *context, double *sum, double *maximum,
    size_t *unresolved, sixel_allocator_t *allocator)
{
    unsigned char *stream;
    unsigned char *pixels;
    float const *source;
    float working[3];
    double error;
    size_t position;
    size_t passes;
    unsigned int flags;
    int width;
    int height;
    int ok;

    stream = pixels = NULL;
    ok = 0;
    *sum = *maximum = 0;
    *unresolved = 0;
    if (sink->bytes > INT_MAX || fflush(sink->file) != 0 ||
            fseek(sink->file, 0, SEEK_SET) != 0) {
        goto end;
    }
    stream = malloc(sink->bytes);
    if (stream == NULL || fread(stream, 1, sink->bytes, sink->file) !=
            sink->bytes || fseek(sink->file, 0, SEEK_END) != 0 ||
            sixel_decode_sequence_mapped(stream, sink->bytes, NULL,
                &pixels, &width, &height, &flags, &passes, allocator)
                != SIXEL_OK || width != context->width ||
                height != context->height) {
        goto end;
    }
    source = (float const *)sixel_frame_get_pixels(context->source);
    for (position = 0; position < context->count; ++position) {
        if (!context->domain[position]) {
            continue;
        }
        if (pixels[position * 4 + 3] != 255 ||
                !sixel_palette_decode_output(context->transform,
                    pixels + position * 4, context->workspace, working)) {
            goto end;
        }
        error = sixel_repair_distance2(source + position * 3, working);
        *sum += error;
        *maximum = fmax(*maximum, error);
        *unresolved += error > context->stop_squared;
    }
    ok = 1;
end:
    free(stream);
    sixel_allocator_free(allocator, pixels);
    return ok;
}

static void
progressive_study_record(sixel_repair_measure_record_t const *record,
    size_t bytes, double seconds)
{
    size_t i;
    size_t j;

    printf("{\"event\":\"step\",\"version\":%u,\"sequence\":%" PRIu64
        ",\"generation\":%" PRIu64 ",\"configuration_revision\":%" PRIu64
        ",\"before_revision\":%" PRIu64 ",\"after_revision\":%" PRIu64
        ",\"status\":%d,\"outcome\":%d,\"audit_complete\":%d"
        ",\"proposal_complete\":%d,\"committed\":%d,\"timing_valid\":%d"
        ",\"wire_bytes\":%lu,\"step_seconds\":%.17g",
        record->version, record->sequence, record->generation,
        record->configuration_revision, record->before_revision,
        record->after_revision, record->status, record->outcome,
        record->audit_complete, record->proposal_complete, record->committed,
        record->timing_valid, (unsigned long)bytes, seconds);
    printf(",\"selection_complete\":%d,\"packet_limit\":%lu"
        ",\"pixel_limit\":%lu,\"distance_limit\":%lu,\"color_limit\":%u",
        record->selection_complete, (unsigned long)record->packet_limit,
        (unsigned long)record->pixel_limit,
        (unsigned long)record->distance_limit, record->color_limit);
    printf(",\"ladder\":{\"allowance\":%u,\"rung\":%u,\"emissions_before\":%u"
        ",\"noise_off\":%d,\"reason\":%u,\"thresholds\":[",
        record->ladder.allowance,
        record->ladder.rung, record->ladder.emissions,
        record->ladder.noise_off, record->ladder.reason);
    for (i = 0; i < record->ladder.count; ++i) {
        printf("%s%.17g", i ? "," : "", record->ladder.thresholds[i]);
    }
    printf("]}");
    printf(",\"workspace\":%d,\"stop_squared\":%.17g"
        ",\"mask_squared\":%.17g,\"full_selection\":%d"
        ",\"zero_noise\":%d,\"rescue\":%d,\"noise_strength\":%.17g"
        ",\"before_sum\":%.17g,\"before_max\":%.17g"
        ",\"before_excess\":%.17g,\"eligible\":%lu,\"invalid\":%lu"
        ",\"unresolved\":%lu,\"selected\":%lu",
        record->workspace, record->stop_squared, record->mask_squared,
        record->full_selection, record->zero_noise, record->rescue,
        record->noise_strength, record->before_sum, record->before_max,
        record->before_excess, (unsigned long)record->eligible,
        (unsigned long)record->invalid, (unsigned long)record->unresolved,
        (unsigned long)record->selected);
    printf(",\"proposal_pixels\":%lu,\"proposal_gain\":%.17g"
        ",\"proposal_bytes\":%lu,\"fitting_retries\":%lu"
        ",\"committed_pixels\":%lu,\"committed_bytes\":%lu"
        ",\"committed_gain\":%.17g,\"committed_excess_gain\":%.17g"
        ",\"resolved_pixels\":%lu,\"constructed_colors\":%u"
        ",\"transmitted_colors\":%u,\"observer_distances\":%" PRIu64,
        (unsigned long)record->proposal_pixels, record->proposal_gain,
        (unsigned long)record->proposal_bytes,
        (unsigned long)record->fitting_retries,
        (unsigned long)record->committed_pixels,
        (unsigned long)record->committed_bytes, record->committed_gain,
        record->committed_excess_gain, (unsigned long)record->resolved_pixels,
        record->constructed_colors, record->transmitted_colors,
        record->observer_distances);
    printf(",\"band_upper\":[");
    for (i = 0; i < SIXEL_REPAIR_MEASURE_BANDS - 1; ++i) {
        printf("%s%.17g", i ? "," : "", record->band_upper[i]);
    }
    printf("],\"bands\":[");
    for (i = 0; i < SIXEL_REPAIR_MEASURE_BANDS; ++i) {
        printf("%s[%lu,%.17g]", i ? "," : "",
            (unsigned long)record->bands[i], record->band_sums[i]);
    }
    printf("],\"transitions\":[");
    for (i = 0; i < SIXEL_REPAIR_MEASURE_BANDS; ++i) {
        printf("%s[", i ? "," : "");
        for (j = 0; j < SIXEL_REPAIR_MEASURE_BANDS; ++j) {
            printf("%s%lu", j ? "," : "",
                (unsigned long)record->transitions[i][j]);
        }
        printf("]");
    }
    printf("],\"sample_stride\":%lu,\"samples\":[",
        (unsigned long)record->sample_stride);
    for (i = 0; i < record->sample_count; ++i) {
        printf("%s[%lu,%.9g,%.9g,%.9g,%.17g]", i ? "," : "",
            (unsigned long)record->samples[i].position,
            record->samples[i].target[0], record->samples[i].target[1],
            record->samples[i].target[2], record->samples[i].error);
    }
    printf("],\"palette\":[");
    for (i = 0; i < record->palette_count; ++i) {
        printf("%s[%.9g,%.9g,%.9g]", i ? "," : "",
            record->palette[i][0], record->palette[i][1],
            record->palette[i][2]);
    }
    printf("],\"phase_cost\":[");
    for (i = 0; i < SIXEL_REPAIR_MEASURE_PHASES; ++i) {
        printf("%s[%" PRIu64 ",%" PRIu64 ",%.17g]", i ? "," : "",
            record->call[i].visits, record->call[i].distances,
            record->call[i].seconds);
    }
    printf("]}\n");
}

typedef struct progressive_study_pixel {
    size_t position;
    double before;
    double after;
    double gain;
    double excess;
    double score;
} progressive_study_pixel_t;

static int
progressive_study_order(void const *left, void const *right)
{
    progressive_study_pixel_t const *a;
    progressive_study_pixel_t const *b;

    a = left;
    b = right;
    if (a->score != b->score) {
        return a->score > b->score ? -1 : 1;
    }
    return a->position < b->position ? -1 : a->position > b->position;
}

/* Offline, single-owner counterfactuals. The completed step leaves audited
 * errors intact until the next audit; its record owns the projected palette.
 * Use both before another step. No encoder state or capture is modified.
 * Full scans and sorts are outside the instrumented encoder time/budgets. */
static int
progressive_study_predict(sixel_progressive_t *context,
    sixel_repair_measure_record_t const *record)
{
    progressive_study_pixel_t *pixels;
    progressive_study_pixel_t *pixel;
    sixel_repair_prediction_t prediction;
    sixel_repair_predict_band_t const *band;
    sixel_repair_predict_point_t const *point;
    float const *source;
    double threshold;
    double stop;
    double nearest;
    double after;
    double gain;
    double excess;
    double maximum;
    double before_sum;
    size_t count;
    size_t position;
    size_t i;
    size_t quota;
    size_t quotas[3] = {0, 16, 256};
    size_t selected;
    size_t painted;
    size_t resolved;
    size_t keep;
    unsigned int color;
    int mask;
    int policy;
    int b;
    int q;
    int ok;
    char const *names[3] = {"tail", "aggregate", "target_excess"};

    if (!record->audit_complete || record->invalid ||
            !record->committed || !record->palette_count ||
            record->generation != context->generation ||
            record->configuration_revision != context->configuration_revision
            || record->before_revision != context->audit.history_revision) {
        return 0;
    }
    if (context->count > SIZE_MAX / sizeof(*pixels)) {
        return 0;
    }
    pixels = malloc(context->count * sizeof(*pixels));
    if (pixels == NULL) {
        return 0;
    }
    count = 0;
    ok = 0;
    before_sum = 0;
    stop = record->stop_squared;
    source = (float const *)sixel_frame_get_pixels(context->source);
    for (position = 0; position < context->count; ++position) {
        if (!context->domain[position]) {
            continue;
        }
        pixel = pixels + count++;
        pixel->position = position;
        pixel->before = context->errors[position];
        pixel->after = pixel->before;
        if (pixel->before > stop) {
            nearest = DBL_MAX;
            for (color = 0; color < record->palette_count; ++color) {
                nearest = fmin(nearest, sixel_repair_distance2(
                    source + position * 3, record->palette[color]));
            }
            if (sixel_repair_improves(pixel->before, nearest)) {
                pixel->after = nearest;
            }
        }
        pixel->gain = pixel->before - pixel->after;
        pixel->excess = fmax(0, pixel->before - stop)
            - fmax(0, pixel->after - stop);
        before_sum += pixel->before;
    }
    if (count != record->eligible || fabs(before_sum - record->before_sum)
            > 1e-7 * fmax(1, before_sum)) {
        goto end;
    }
    printf("{\"event\":\"distribution\",\"sequence\":%" PRIu64
        ",\"generation\":%" PRIu64 ",\"before_revision\":%" PRIu64
        ",\"bands\":[", record->sequence, record->generation,
        record->before_revision);
    for (b = 0; b < SIXEL_REPAIR_PREDICT_BANDS; ++b) {
        band = record->prediction_model.bands + b;
        printf("%s{\"population\":%lu,\"sum\":%.17g,\"maximum\":%.17g,"
            "\"samples\":[", b ? "," : "", (unsigned long)band->population,
            band->sum, band->maximum);
        for (i = 0; i < band->count; ++i) {
            point = band->points + i;
            printf("%s[%lu,%.9g,%.9g,%.9g,%.17g]", i ? "," : "",
                (unsigned long)point->position, point->target[0],
                point->target[1], point->target[2], point->error);
        }
        printf("]}");
    }
    printf("]}\n");
    for (mask = 0; mask < 2; ++mask) {
        threshold = mask ? stop : record->mask_squared;
        if (mask && threshold == record->mask_squared) {
            continue;
        }
        gain = excess = maximum = 0;
        selected = painted = resolved = 0;
        for (i = 0; i < count; ++i) {
            pixel = pixels + i;
            after = pixel->before;
            if (pixel->before > stop && pixel->before >= threshold) {
                ++selected;
                after = pixel->after;
                painted += pixel->gain > 0;
                resolved += after <= stop;
                gain += pixel->gain;
                excess += pixel->excess;
            }
            maximum = fmax(maximum, after);
        }
        /* Match the actual proposal only for the model's stated no-dither
         * nearest-palette case. Rescue paints use a different assignment. */
        if (!mask && !record->noise_strength && !record->rescue &&
                (painted != record->proposal_pixels ||
                 fabs(gain - record->proposal_gain) >
                    1e-7 * fmax(1, gain))) {
            goto end;
        }
        for (q = 0; q < 3; ++q) {
            quota = quotas[q];
            if (sixel_repair_predict(&record->prediction_model,
                    record->palette, record->palette_count, threshold,
                    quota, &prediction) != SIXEL_OK) {
                goto end;
            }
            if (prediction.maximum_lower > maximum + 1e-12 ||
                    prediction.maximum_upper + 1e-12 < maximum) {
                goto end;
            }
            printf("{\"event\":\"prediction\",\"sequence\":%" PRIu64
                ",\"generation\":%" PRIu64 ",\"before_revision\":%" PRIu64
                ",\"kind\":\"no_dither_uncapped_fixed_palette\""
                ",\"mask_squared\":%.17g,\"quota\":%lu,\"samples\":%lu"
                ",\"distances\":%" PRIu64 ",\"exact\":%d"
                ",\"gain\":%.17g,\"excess_gain\":%.17g"
                ",\"selected\":%.17g,\"painted\":%.17g,\"resolved\":%.17g"
                ",\"maximum_lower\":%.17g,\"maximum_upper\":%.17g"
                ",\"reference_gain\":%.17g,\"reference_excess\":%.17g"
                ",\"reference_selected\":%lu,\"reference_painted\":%lu"
                ",\"reference_resolved\":%lu,\"reference_maximum\":%.17g}\n",
                record->sequence, record->generation, record->before_revision,
                threshold, (unsigned long)quota,
                (unsigned long)prediction.samples, prediction.distances,
                prediction.exact, prediction.gain, prediction.excess_gain,
                prediction.selected, prediction.painted, prediction.resolved,
                prediction.maximum_lower, prediction.maximum_upper,
                gain, excess, (unsigned long)selected, (unsigned long)painted,
                (unsigned long)resolved, maximum);
        }
    }
    /* Equal painted-pixel caps isolate the priority objective on precisely
     * the same source/history/palette. They are NOT equal byte budgets and
     * do not measure palette reconstruction or future continuation. */
    keep = record->committed_pixels;
    for (policy = 0; policy < 3; ++policy) {
        for (i = 0; i < count; ++i) {
            pixel = pixels + i;
            pixel->score = pixel->gain <= 0 ? -1 : policy == 0
                ? pixel->before : policy == 1 ? pixel->gain : pixel->excess;
        }
        qsort(pixels, count, sizeof(*pixels), progressive_study_order);
        gain = excess = maximum = 0;
        painted = resolved = 0;
        for (i = 0; i < count; ++i) {
            pixel = pixels + i;
            after = pixel->before;
            if (i < keep && pixel->score >= 0) {
                after = pixel->after;
                ++painted;
                resolved += after <= stop;
                gain += pixel->gain;
                excess += pixel->excess;
            }
            maximum = fmax(maximum, after);
        }
        printf("{\"event\":\"priority\",\"sequence\":%" PRIu64
            ",\"generation\":%" PRIu64 ",\"before_revision\":%" PRIu64
            ",\"kind\":\"no_dither_fixed_palette_equal_paint_cap\""
            ",\"policy\":\"%s\",\"paint_cap\":%lu,\"painted\":%lu"
            ",\"gain\":%.17g,\"excess_gain\":%.17g,\"resolved\":%lu"
            ",\"maximum\":%.17g,\"wire_bytes\":null}\n",
            record->sequence, record->generation, record->before_revision,
            names[policy], (unsigned long)keep, (unsigned long)painted,
            gain, excess, (unsigned long)resolved, maximum);
    }
    ok = 1;
end:
    free(pixels);
    return ok;
}

#define STUDY_REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "progressive-study: failed at line %d: %s\n", \
            __LINE__, sixel_helper_get_additional_message()); \
        goto end; \
    } \
} while (0)

int
main(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    sixel_loader_t *loader;
    sixel_allocator_t *allocator;
    sixel_frame_t *source;
    sixel_frame_interface_t *baseline;
    sixel_frame_interface_t *source_if;
    sixel_output_t *output;
    sixel_progressive_t *context;
    sixel_progressive_config_t config;
    sixel_repair_work_t work;
    sixel_repair_result_t result;
    sixel_repair_measure_record_t record;
    progressive_study_sink_t sink;
    double limit;
    double start;
    double seconds;
    double used;
    double before;
    double after;
    double maximum;
    double noise;
    double fraction;
    unsigned long cap;
    unsigned long packet_limit;
    unsigned long emitted;
    unsigned long color_cap;
    unsigned long stage_passes;
    size_t unresolved;
    char *tail;
    int yes;
    int no;
    int ok;
    int ladder;
    SIXELSTATUS status;

    encoder = NULL;
    loader = NULL;
    allocator = NULL;
    source = NULL;
    baseline = NULL;
    output = NULL;
    context = NULL;
    memset(&sink, 0, sizeof(sink));
    yes = 1;
    no = ok = 0;
    emitted = 0;
    used = 0;
    if (argc != 8 && argc != 10 && argc != 12) {
        fprintf(stderr, "usage: progressive-study INPUT CAPTURE.six "
            "MAX_REPAIRS MAX_REPAIR_SECONDS NOISE PACKET_BYTES WORKSPACE "
            "[SELECTION_FRACTION COLORS [legacy|ladder STAGE_PASSES]]\n");
        return 2;
    }
    cap = strtoul(argv[3], &tail, 10);
    STUDY_REQUIRE(*tail == 0 && cap > 0 && cap <= 10000);
    limit = strtod(argv[4], &tail);
    STUDY_REQUIRE(tail != argv[4] && *tail == 0 &&
        isfinite(limit) && limit > 0);
    noise = strtod(argv[5], &tail);
    STUDY_REQUIRE(tail != argv[5] && *tail == 0 && isfinite(noise) &&
        noise >= 0 && noise <= 1);
    packet_limit = strtoul(argv[6], &tail, 10);
    STUDY_REQUIRE(*tail == 0 && packet_limit > 0 && packet_limit <= INT_MAX);
    fraction = 0.25;
    color_cap = 32;
    ladder = 0;
    stage_passes = 2;
    if (argc >= 10) {
        fraction = strtod(argv[8], &tail);
        STUDY_REQUIRE(tail != argv[8] && *tail == 0 && isfinite(fraction) &&
            fraction > 0 && fraction <= 1);
        color_cap = strtoul(argv[9], &tail, 10);
        STUDY_REQUIRE(*tail == 0 && color_cap > 0 && color_cap <= 255);
    }
    if (argc == 12) {
        ladder = strcmp(argv[10], "ladder") == 0;
        STUDY_REQUIRE(ladder || strcmp(argv[10], "legacy") == 0);
        stage_passes = strtoul(argv[11], &tail, 10);
        STUDY_REQUIRE(*tail == 0 && (stage_passes == 1 || stage_passes == 2
            || stage_passes == 4));
        STUDY_REQUIRE(!ladder || fraction == 0.25);
    }
    STUDY_REQUIRE(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)
        == SIXEL_OK);
    STUDY_REQUIRE(sixel_loader_new(&loader, allocator) == SIXEL_OK);
    STUDY_REQUIRE(sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
        &source) == SIXEL_OK);
    STUDY_REQUIRE(sixel_loader_setopt(loader,
        SIXEL_LOADER_OPTION_REQUIRE_STATIC, &yes) == SIXEL_OK);
    STUDY_REQUIRE(sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE,
        &no) == SIXEL_OK);
    STUDY_REQUIRE(sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
        "builtin!") == SIXEL_OK);
    STUDY_REQUIRE(sixel_loader_load_file(loader, argv[1],
        progressive_study_load) == SIXEL_OK && source != NULL);
    source_if = sixel_frame_as_interface(source);
    STUDY_REQUIRE(source_if->vtbl->clone(source_if, allocator, &baseline)
        == SIXEL_OK);
    STUDY_REQUIRE(sixel_encoder_new(&encoder, allocator) == SIXEL_OK);
    STUDY_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta") == SIXEL_OK);
    STUDY_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        "off") == SIXEL_OK);
    STUDY_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADING_POLICY,
        "balanced:B1") == SIXEL_OK);
    STUDY_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_LUT_POLICY,
        "none") == SIXEL_OK);
    STUDY_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
        "none") == SIXEL_OK);
    STUDY_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT_COLORSPACE,
        "gamma") == SIXEL_OK);
    STUDY_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_WORKING_COLORSPACE, argv[7]) == SIXEL_OK);
    sink.file = sixel_compat_fopen(argv[2], "w+b");
    STUDY_REQUIRE(sink.file != NULL);
    sink.anchor = '7';
    STUDY_REQUIRE(sixel_output_new(&output, progressive_study_write, &sink,
        allocator) == SIXEL_OK);
    sixel_encoder_core_require_keep_header(output, 1);
    start = sixel_timer_now();
    STUDY_REQUIRE(sixel_encoder_encode_frame(encoder,
        (sixel_frame_t *)baseline, output) == SIXEL_OK);
    seconds = sixel_timer_now() - start;
    STUDY_REQUIRE(isfinite(seconds) && seconds >= 0);
    STUDY_REQUIRE(sixel_progressive_config_init(&config, sizeof(config))
        == SIXEL_OK);
    config.noise_strength = noise;
    config.selection_fraction = fraction;
    config.max_colors = (unsigned int)color_cap;
    STUDY_REQUIRE(sixel_progressive_new(&context, encoder, &config, allocator)
        == SIXEL_OK);
    if (ladder) {
        STUDY_REQUIRE(sixel_repair_ladder_enable(context,
            (unsigned int)stage_passes) == SIXEL_OK);
    }
    STUDY_REQUIRE(sixel_repair_measure_enable(context) == SIXEL_OK);
    STUDY_REQUIRE(sixel_progressive_bind_frame(context, source, 0, 0, 1)
        == SIXEL_OK);
    STUDY_REQUIRE(progressive_study_observe(&sink, context, &before,
        &maximum, &unresolved, allocator));
    printf("{\"event\":\"baseline\",\"version\":1,\"wire_bytes\":%lu,"
        "\"seconds\":%.17g,\"sum\":%.17g,\"max\":%.17g,"
        "\"unresolved\":%lu,\"width\":%d,\"height\":%d}\n",
        (unsigned long)sink.bytes, seconds, before, maximum,
        (unsigned long)unresolved, context->width, context->height);
    memset(&result, 0, sizeof(result));
    while (emitted < cap && used < limit) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = 1048576;
        work.distance_limit = 8388608;
        sink.anchor = '8';
        start = sixel_timer_now();
        status = sixel_repair_context_step(context, &work, packet_limit,
            output, &result);
        seconds = sixel_timer_now() - start;
        STUDY_REQUIRE(isfinite(seconds) && seconds >= 0);
        used += seconds;
        STUDY_REQUIRE(sixel_repair_measure_read(context, &record) == SIXEL_OK);
        progressive_study_record(&record, sink.bytes, seconds);
        STUDY_REQUIRE(status == SIXEL_OK);
        if (result.audit.outcome == SIXEL_REPAIR_FRAME_EMITTED) {
            ++emitted;
            STUDY_REQUIRE(progressive_study_predict(context, &record));
            STUDY_REQUIRE(progressive_study_observe(&sink, context, &after,
                &maximum, &unresolved, allocator));
            STUDY_REQUIRE(fabs(before - after - record.committed_gain) <=
                1e-7 * fmax(1, before));
            printf("{\"event\":\"decoded\",\"repair\":%lu,"
                "\"wire_bytes\":%lu,\"sum\":%.17g,\"max\":%.17g,"
                "\"unresolved\":%lu,\"gain_error\":%.17g}\n",
                emitted, (unsigned long)sink.bytes, after, maximum,
                (unsigned long)unresolved,
                before - after - record.committed_gain);
            before = after;
        } else if (result.audit.outcome != SIXEL_REPAIR_YIELDED) {
            break;
        }
    }
    printf("{\"event\":\"end\",\"outcome\":%d,\"repairs\":%lu,"
        "\"wire_bytes\":%lu,\"repair_seconds\":%.17g,\"censored\":%d}\n",
        result.audit.outcome, emitted, (unsigned long)sink.bytes, used,
        result.audit.outcome == SIXEL_REPAIR_FRAME_EMITTED ||
        result.audit.outcome == SIXEL_REPAIR_YIELDED);
    STUDY_REQUIRE(!ferror(stdout) && fflush(sink.file) == 0);
    ok = 1;
end:
    if (sink.file != NULL && fclose(sink.file) != 0) {
        ok = 0;
    }
    sixel_output_unref(output);
    sixel_progressive_unref(context);
    sixel_frame_unref((sixel_frame_t *)baseline);
    sixel_frame_unref(source);
    sixel_encoder_unref(encoder);
    sixel_loader_unref(loader);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}
