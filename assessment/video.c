/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <windows.h>
#endif
#include "video.h"
#include "persistence.h"
#include "color.h"
#include "loader.h"
#include "chunk-factory.h"
#include "chunk-view.h"
#include "sixel_decode_pixels.h"

/* The loader pushes frames; SIXEL is pulled at frame or clock boundaries.
 * Only one retained display canvas and one pair of assessment images exist.
 * Reports are spooled until EOF validates both inputs, never silently zipped
 * to the shorter input. Only the current error plane and ages stay in RAM;
 * optional map exports are spooled to disk. */
typedef struct lsqa_video_state {
    lsqa_video_options_t const *options;
    sixel_allocator_t *allocator;
    sixel_sequence_decoder_t *decoder;
    sixel_assessment_t *qa;
    sixel_color_def_t *definition;
    sixel_color_def_t *srgb;
    int intent;
    FILE *timeline;
    FILE *display;
    size_t update;
    size_t next_end;
    double next_pts;
    int pending;
    FILE *report;
    FILE *maps;
    unsigned char const *rgba;
    double *errors;
    double *scratch;
    double *ages;
    lsqa_persistence_result_t persistence;
    float *rgb;
    size_t pixels;
    int width;
    int height;
    size_t frame;
    size_t samples;
    size_t reported_end;
    size_t reported_packets;
    size_t late_updates;
    double blank_duration;
    size_t packets;
    size_t byte_end;
    size_t input_size;
    uint64_t map_bytes;
    double pts;
    double averages[SIXEL_ASSESSMENT_METRIC_COUNT];
    double minima[SIXEL_ASSESSMENT_METRIC_COUNT];
    double maxima[SIXEL_ASSESSMENT_METRIC_COUNT];
    unsigned int valid_mask;
    unsigned int finite_mask;
} lsqa_video_state_t;

static SIXELSTATUS
lsqa_video_error(char const *message)
{
    sixel_helper_set_additional_message(message);
    return SIXEL_BAD_INPUT;
}

static void
lsqa_video_string(FILE *file, char const *text)
{
    unsigned char const *p;

    fputc('"', file);
    for (p = (unsigned char const *)text; *p != 0; ++p) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', file);
            fputc(*p, file);
        } else if (*p < 32) {
            fprintf(file, "\\u%04x", (unsigned int)*p);
        } else {
            fputc(*p, file);
        }
    }
    fputc('"', file);
}

static void
lsqa_video_number(FILE *file, double value)
{
    if (isfinite(value)) {
        fprintf(file, "%.17g", value);
    } else {
        fputs("null", file);
    }
}

static void
lsqa_video_metrics(FILE *file, double const *values, unsigned int mask)
{
    int i;
    int comma;
    char const *name;

    comma = 0;
    fputc('{', file);
    for (i = 0; i < SIXEL_ASSESSMENT_METRIC_COUNT; ++i) {
        name = sixel_assessment_metric_name(i);
        if (name == NULL || (mask & (1u << i)) == 0) {
            continue;
        }
        if (comma) {
            fputc(',', file);
        }
        comma = 1;
        lsqa_video_string(file, name);
        fputc(':', file);
        lsqa_video_number(file, values[i]);
    }
    fputc('}', file);
}

/* Strict numeric JSONL records, independent of member order.
 * Byte offsets fit the capture decoder's INT_MAX input limit. Reject unknown
 * fields, duplicate keys, non-JSON numbers and incomplete lines explicitly. */
static int
lsqa_video_record(FILE *file, char const *const *names, int count,
    double *values)
{
    char line[1024];
    char key[32];
    char *p;
    char *end;
    char *number;
    unsigned int seen;
    size_t length;
    int i;

    if (fgets(line, sizeof(line), file) == NULL) {
        return ferror(file) ? -1 : 0;
    }
    if (strchr(line, '\n') == NULL && !feof(file)) {
        return -1;
    }
    p = line;
    seen = 0;
    while (*p == ' ' || *p == '\t') { ++p; }
    if (*p++ != '{') { return -1; }
    for (;;) {
        while (*p == ' ' || *p == '\t') { ++p; }
        if (*p++ != '"') { return -1; }
        end = strchr(p, '"');
        if (end == NULL) { return -1; }
        length = (size_t)(end - p);
        if (length >= sizeof(key)) { return -1; }
        memcpy(key, p, length);
        key[length] = '\0';
        for (i = 0; i < count && strcmp(key, names[i]) != 0; ++i) { }
        if (i == count || (seen & (1u << i)) != 0) { return -1; }
        seen |= 1u << i;
        p = end + 1;
        while (*p == ' ' || *p == '\t') { ++p; }
        if (*p++ != ':') { return -1; }
        while (*p == ' ' || *p == '\t') { ++p; }
        number = p;
        if (*p == '-') { ++p; }
        if (*p < '0' || *p > '9') { return -1; }
        if (*p == '0') {
            ++p;
        } else {
            while (*p >= '0' && *p <= '9') { ++p; }
        }
        if (*p == '.') {
            ++p;
            if (*p < '0' || *p > '9') { return -1; }
            while (*p >= '0' && *p <= '9') { ++p; }
        }
        if (*p == 'e' || *p == 'E') {
            ++p;
            if (*p == '+' || *p == '-') { ++p; }
            if (*p < '0' || *p > '9') { return -1; }
            while (*p >= '0' && *p <= '9') { ++p; }
        }
        errno = 0;
        values[i] = strtod(number, &end);
        if (errno != 0 || end != p || !isfinite(values[i])
                || values[i] < 0) { return -1; }
        while (*p == ' ' || *p == '\t') { ++p; }
        if (*p == '}') {
            ++p;
            break;
        }
        if (*p++ != ',') { return -1; }
    }
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') { ++p; }
    return *p == '\0' && seen == (1u << count) - 1u ? 1 : -1;
}

static int
lsqa_video_timeline(FILE *file, size_t frame, size_t *byte_end,
    double *pts, double *duration)
{
    static char const *const names[] = {
        "frame", "byte_end", "pts", "duration"
    };
    double values[4];
    int result;

    result = lsqa_video_record(file, names, 4, values);
    if (result != 1) { return result; }
    if (values[0] != (double)frame || values[1] > INT_MAX
            || floor(values[1]) != values[1] || values[3] <= 0
            || !isfinite(values[2] + values[3])) { return -1; }
    *byte_end = (size_t)values[1];
    *pts = values[2];
    *duration = values[3];
    return 1;
}

/* The display sidecar declares availability of complete packet ranges.
 * It does not assign source-frame identities or retime the reference. One
 * pending event suffices; the capture is decoded only when it becomes due. */
static SIXELSTATUS
lsqa_video_event(lsqa_video_state_t *v)
{
    static char const *const names[] = {"update", "byte_end", "pts"};
    double values[3];
    int result;

    result = lsqa_video_record(v->display, names, 3, values);
    v->pending = 0;
    if (result == 0) { return SIXEL_OK; }
    if (result != 1 || values[0] != (double)v->update
            || values[1] > (double)v->input_size
            || values[1] <= (double)v->next_end
            || floor(values[1]) != values[1] || values[2] < v->next_pts) {
        return lsqa_video_error("invalid display timeline");
    }
    ++v->update;
    v->next_end = (size_t)values[1];
    v->next_pts = values[2];
    v->pending = 1;
    return SIXEL_OK;
}

static SIXELSTATUS
lsqa_video_pull(lsqa_video_state_t *v, size_t end, int single)
{
    SIXELSTATUS status;
    unsigned char const *rgba;
    int width;
    int height;

    while (single || v->byte_end < end) {
        status = sixel_sequence_next(v->decoder, &rgba, &width, &height,
            &v->byte_end);
        if (status != SIXEL_OK) {
            return lsqa_video_error(
                "target has fewer frames or malformed SIXEL packets");
        }
        if (v->pixels != 0 && (width != v->width || height != v->height)) {
            return lsqa_video_error("video canvas dimensions must match");
        }
        v->rgba = rgba;
        v->width = width;
        v->height = height;
        ++v->packets;
        if (single) { break; }
        if (v->byte_end > end) {
            return lsqa_video_error("timeline byte_end splits a packet");
        }
    }
    return SIXEL_OK;
}

/* Exact nearest-rank percentile. Three-way selection handles equal errors
 * without quadratic scans over constant fields, common in flat imagery. */
static double
lsqa_video_percentile(double *values, size_t count)
{
    size_t lo;
    size_t hi;
    size_t lt;
    size_t gt;
    size_t i;
    size_t rank;
    double pivot;
    double swap;

    rank = count - count / 20U - 1U; /* ceil(0.95 * count) - 1 */
    lo = 0;
    hi = count;
    while (hi - lo > 1) {
        pivot = values[lo + (hi - lo) / 2];
        lt = lo;
        gt = hi;
        i = lo;
        while (i < gt) {
            if (values[i] < pivot) {
                swap = values[lt];
                values[lt++] = values[i];
                values[i++] = swap;
            } else if (values[i] > pivot) {
                swap = values[--gt];
                values[gt] = values[i];
                values[i] = swap;
            } else {
                ++i;
            }
        }
        if (rank < lt) {
            hi = lt;
        } else if (rank >= gt) {
            lo = gt;
        } else {
            return pivot;
        }
    }
    return values[rank];
}

static SIXELSTATUS
lsqa_video_map(lsqa_video_state_t *v)
{
    unsigned char block[4096];
    uint32_t bits;
    float value;
    size_t i;
    size_t j;
    size_t count;

    if (sizeof(float) != 4 || FLT_RADIX != 2 || FLT_MANT_DIG != 24) {
        return SIXEL_NOT_IMPLEMENTED;
    }
    if ((uint64_t)v->pixels > (UINT64_MAX - v->map_bytes) / 4U) {
        return SIXEL_BAD_INPUT;
    }
    for (i = 0; i < v->pixels; i += count) {
        count = v->pixels - i;
        if (count > sizeof(block) / 4U) { count = sizeof(block) / 4U; }
        for (j = 0; j < count; ++j) {
            value = (float)v->errors[i + j];
            if (!isfinite(value)) {
                return lsqa_video_error("S-CIELAB map exceeds float32 range");
            }
            memcpy(&bits, &value, 4);
            block[j * 4] = (unsigned char)bits;
            block[j * 4 + 1] = (unsigned char)(bits >> 8);
            block[j * 4 + 2] = (unsigned char)(bits >> 16);
            block[j * 4 + 3] = (unsigned char)(bits >> 24);
        }
        if (fwrite(block, 4U, count, v->maps) != count) {
            return SIXEL_RUNTIME_ERROR;
        }
    }
    v->map_bytes += (uint64_t)v->pixels * 4U;
    return SIXEL_OK;
}

/* Convert the retained original paint samples before measurement. In
 * particular, never reinterpret an already flattened sRGB background as P3.
 * Extended linear coordinates remain unclipped, matching still assessment. */
static SIXELSTATUS
lsqa_video_target(lsqa_video_state_t *v, sixel_frame_t **frame)
{
    double sample[3];
    double xyz[3];
    double encoded[3];
    float background[3];
    size_t i;
    int c;
    SIXELSTATUS status;

    for (c = 0; c < 3; ++c) {
        background[c] = (float)sixel_color_decode_srgb(
            v->options->background == NULL ? 0.0
                : v->options->background[c] / 255.0);
    }
    for (i = 0; i < v->pixels; ++i) {
        if (v->rgba == NULL || v->rgba[i * 4 + 3] == 0) {
            memcpy(v->rgb + i * 3, background, sizeof(background));
            continue;
        }
        for (c = 0; c < 3; ++c) { sample[c] = v->rgba[i * 4 + c] / 255.0; }
        if (!sixel_cdef_to_xyz(v->definition, sample, xyz, v->intent)
                || !sixel_cdef_from_xyz(v->srgb, xyz, encoded, 1)) {
            return SIXEL_BAD_INPUT;
        }
        for (c = 0; c < 3; ++c) {
            v->rgb[i * 3 + c] =
                (float)sixel_color_decode_srgb(encoded[c]);
        }
    }
    status = sixel_frame_new(frame, v->allocator);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_init_borrowed(*frame, v->rgb,
            v->width, v->height, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
            NULL, 0);
    }
    return status;
}

static SIXELSTATUS
lsqa_video_measure(sixel_frame_t *reference, lsqa_video_state_t *v,
    double pts, double duration)
{
    sixel_assessment_t *qa;
    sixel_frame_t *target;
    sixel_frame_t *ref_copy;
    sixel_frame_interface_t *ref_interface;
    lsqa_persistence_result_t persistence;
    unsigned char background[3];
    SIXELSTATUS status;
    size_t above;
    size_t i;
    double percentile;
    double weight;
    int bit;
    int channel;

    qa = v->qa;
    target = NULL;
    ref_copy = NULL;
    ref_interface = NULL;
    /* A delayed first update exposes the initial background. Its geometry
     * comes from the reference, and the first packet must agree with it. */
    if (v->pixels == 0 && v->rgba == NULL) {
        v->width = sixel_frame_get_width(reference);
        v->height = sixel_frame_get_height(reference);
    }
    if (sixel_frame_get_width(reference) != v->width
            || sixel_frame_get_height(reference) != v->height) {
        return lsqa_video_error("video canvas dimensions must match");
    }
    if (v->pixels == 0) {
        v->pixels = (size_t)v->width * (size_t)v->height;
        if (v->pixels > SIZE_MAX / (3U * sizeof(float))
                || v->pixels > SIZE_MAX / sizeof(double)) {
            return SIXEL_BAD_ALLOCATION;
        }
        v->rgb = malloc(v->pixels * 3U * sizeof(float));
        if (v->report != NULL) {
            v->errors = malloc(v->pixels * sizeof(double));
            v->scratch = malloc(v->pixels * sizeof(double));
            v->ages = calloc(v->pixels, sizeof(double));
        }
        if (v->rgb == NULL || (v->report != NULL
                && (v->errors == NULL || v->scratch == NULL
                    || v->ages == NULL))) {
            return SIXEL_BAD_ALLOCATION;
        }
    }
    status = lsqa_video_target(v, &target);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    /* Alpha stripping mutates frames. Loader canvases can be borrowed and
     * reused for disposal/composition, so assessment owns a separate copy. */
    status = sixel_frame_as_interface(reference)->vtbl->clone(
        sixel_frame_as_interface(reference), v->allocator, &ref_interface);
    ref_copy = (sixel_frame_t *)ref_interface;
    if (SIXEL_FAILED(status)) { goto cleanup; }
    memset(background, 0, sizeof(background));
    if (v->options->background != NULL) {
        memcpy(background, v->options->background, sizeof(background));
    }
    status = sixel_frame_strip_alpha(ref_copy, background);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    qa->signal_space = v->options->signal_space < 0
        ? sixel_frame_get_colorspace(reference) : v->options->signal_space;
    qa->signal_bytes = v->options->signal_bytes < 0
        ? !SIXEL_PIXELFORMAT_IS_FLOAT32(sixel_frame_get_pixelformat(reference))
        : v->options->signal_bytes;
    qa->scielab_map = v->errors;
    qa->scielab_map_capacity = v->pixels;
    status = sixel_assessment_analyze(qa, ref_copy, target);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    if (v->samples == 0) {
        v->valid_mask = qa->results_valid_mask;
        v->finite_mask = qa->results_valid_mask;
    }
    v->valid_mask &= qa->results_valid_mask;
    weight = duration / (pts + duration);
    for (channel = 0; channel < SIXEL_ASSESSMENT_METRIC_COUNT; ++channel) {
        bit = (int)(1u << channel);
        if ((qa->results_valid_mask & (unsigned int)bit) == 0) { continue; }
        if (!isfinite(qa->results[channel])) {
            v->finite_mask &= ~(unsigned int)bit;
        }
        /* A weighted running mean avoids overflowing value * duration for
         * valid but very long timelines. Do not discard nonfinite frames. */
        if (v->samples == 0) {
            v->averages[channel] = qa->results[channel];
        } else if (isfinite(v->averages[channel])
                && isfinite(qa->results[channel])) {
            v->averages[channel] = v->averages[channel] * (1.0 - weight)
                + qa->results[channel] * weight;
        } else {
            v->averages[channel] += qa->results[channel];
        }
        if (v->samples == 0 || qa->results[channel] < v->minima[channel]) {
            v->minima[channel] = qa->results[channel];
        }
        if (v->samples == 0 || qa->results[channel] > v->maxima[channel]) {
            v->maxima[channel] = qa->results[channel];
        }
    }
    if (v->report != NULL) {
        above = 0;
        for (i = 0; i < v->pixels; ++i) {
            if (v->errors[i] > v->options->threshold) { ++above; }
        }
        memcpy(v->scratch, v->errors, v->pixels * sizeof(double));
        percentile = lsqa_video_percentile(v->scratch, v->pixels);
        fprintf(v->report, "{\"type\":\"%s\",\"frame\":%zu,",
            v->options->clock_alignment ? "segment" : "frame", v->frame);
        if (v->options->clock_alignment) {
            fprintf(v->report, "\"segment\":%zu,", v->samples);
        }
        fprintf(v->report,
            "\"pts\":%.17g,\"duration\":%.17g,\"width\":%d,\"height\":%d,"
            "\"byte_start\":%zu,\"byte_end\":%zu,\"bytes\":%zu,"
            "\"cumulative_bytes\":%zu,\"packets\":%zu,"
            "\"cumulative_packets\":%zu,\"quality\":",
            pts, duration, v->width, v->height, v->reported_end, v->byte_end,
            v->byte_end - v->reported_end, v->byte_end,
            v->packets - v->reported_packets, v->packets);
        lsqa_video_metrics(v->report, qa->results, qa->results_valid_mask);
        fprintf(v->report, ",\"scielab\":{\"mean\":%.17g,\"max\":%.17g,"
            "\"p95\":%.17g,\"threshold\":%.17g,\"above_count\":%zu,"
            "\"above_fraction\":%.17g}",
            qa->results[SIXEL_ASSESSMENT_INDEX(
                SIXEL_ASSESSMENT_METRIC_SCIELAB)],
            qa->results[SIXEL_ASSESSMENT_INDEX(
                SIXEL_ASSESSMENT_METRIC_SCIELAB_MAX)],
            percentile, v->options->threshold, above,
            (double)above / (double)v->pixels);
        lsqa_persistence_step(v->errors, v->ages, v->pixels,
            duration, v->options->threshold, v->options->persistence_time,
            &persistence);
        v->persistence.weighted_excess =
            v->persistence.weighted_excess * (1.0 - weight)
            + persistence.weighted_excess * weight;
        v->persistence.persistent_fraction =
            v->persistence.persistent_fraction * (1.0 - weight)
            + persistence.persistent_fraction * weight;
        if (persistence.max_age > v->persistence.max_age) {
            v->persistence.max_age = persistence.max_age;
        }
        fprintf(v->report, ",\"persistence\":{\"weighted_excess\":%.17g,"
            "\"persistent_fraction\":%.17g,\"max_age\":%.17g}",
            persistence.weighted_excess, persistence.persistent_fraction,
            persistence.max_age);
        if (v->maps != NULL) {
            fprintf(v->report, ",\"error_map\":{\"offset\":%llu,"
                "\"bytes\":%llu}", (unsigned long long)v->map_bytes,
                (unsigned long long)v->pixels * 4U);
        }
        fputs("}\n", v->report);
        if (ferror(v->report)) { status = SIXEL_RUNTIME_ERROR; goto cleanup; }
    }
    if (v->maps != NULL) {
        status = lsqa_video_map(v);
        if (SIXEL_FAILED(status)) { goto cleanup; }
    }
    ++v->samples;
    v->reported_end = v->byte_end;
    v->reported_packets = v->packets;
    if (v->rgba == NULL) { v->blank_duration += duration; }
    v->pts = pts + duration;
cleanup:
    sixel_frame_unref(ref_copy);
    sixel_frame_unref(target);
    return status;
}

/* Integrate the intersection of piecewise-constant reference and display
 * intervals. An event at a boundary applies to the interval on its right.
 * Equal-time updates are applied together, without zero-duration samples. */
static SIXELSTATUS
lsqa_video_frame(sixel_frame_t *reference, void *context)
{
    lsqa_video_state_t *v;
    SIXELSTATUS status;
    size_t end;
    double pts;
    double duration;
    double stop;
    double boundary;

    v = context;
    pts = v->pts;
    duration = v->options->reference_fps > 0
        ? 1.0 / v->options->reference_fps
        : sixel_frame_get_delay(reference) / 100.0;
    end = v->byte_end;
    if (v->timeline != NULL) {
        if (lsqa_video_timeline(v->timeline, v->frame, &end,
                &pts, &duration) != 1 || end < v->byte_end
                || end > v->input_size
                || fabs(pts - v->pts) > 1e-9 * (1.0 + v->pts)) {
            return lsqa_video_error("invalid or incomplete target timeline");
        }
        /* Accepted timestamp roundoff must not insert unmeasured gaps. */
        pts = v->pts;
    }
    stop = pts + duration;
    if (!(duration > 0) || !isfinite(stop) || stop <= pts) {
        return lsqa_video_error(
            "missing/invalid frame duration; specify --reference-fps");
    }
    if (!v->options->clock_alignment) {
        status = lsqa_video_pull(v, end, v->timeline == NULL);
        if (SIXEL_FAILED(status)) { return status; }
        if (v->rgba == NULL) {
            return lsqa_video_error(
                "first frame must supply a display baseline");
        }
        status = lsqa_video_measure(reference, v, pts, duration);
        if (SIXEL_FAILED(status)) { return status; }
    } else {
        while (pts < stop) {
            while (v->pending && v->next_pts <= pts) {
                status = lsqa_video_pull(v, v->next_end, 0);
                if (SIXEL_FAILED(status)) { return status; }
                status = lsqa_video_event(v);
                if (SIXEL_FAILED(status)) { return status; }
            }
            boundary = v->pending && v->next_pts < stop
                ? v->next_pts : stop;
            status = lsqa_video_measure(reference, v, pts, boundary - pts);
            if (SIXEL_FAILED(status)) { return status; }
            pts = boundary;
        }
    }
    ++v->frame;
    return SIXEL_OK;
}

static int
lsqa_video_distinct(char const *output, char const *input)
{
#if defined(_WIN32)
    HANDLE a;
    HANDLE b;
    BY_HANDLE_FILE_INFORMATION ai;
    BY_HANDLE_FILE_INFORMATION bi;
    int distinct;
#else
    struct stat a;
    struct stat b;
#endif

    if (output == NULL || input == NULL) { return 1; }
    if (strcmp(output, "-") == 0 || strcmp(output, input) == 0) { return 0; }
#if defined(_WIN32)
    /* Windows CRT stat() does not supply unique inode numbers. Compare the
     * file identities instead, including hard links and existing symlinks. */
    a = CreateFileA(output, 0, FILE_SHARE_READ | FILE_SHARE_WRITE
        | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    b = CreateFileA(input, 0, FILE_SHARE_READ | FILE_SHARE_WRITE
        | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    distinct = 1;
    if (a != INVALID_HANDLE_VALUE && b != INVALID_HANDLE_VALUE) {
        distinct = GetFileInformationByHandle(a, &ai)
            && GetFileInformationByHandle(b, &bi)
            && (ai.dwVolumeSerialNumber != bi.dwVolumeSerialNumber
                || ai.nFileIndexHigh != bi.nFileIndexHigh
                || ai.nFileIndexLow != bi.nFileIndexLow);
    }
    if (a != INVALID_HANDLE_VALUE) { CloseHandle(a); }
    if (b != INVALID_HANDLE_VALUE) { CloseHandle(b); }
    return distinct;
#else
    return stat(output, &a) != 0 || stat(input, &b) != 0
        || a.st_dev != b.st_dev || a.st_ino != b.st_ino;
#endif
}

static SIXELSTATUS
lsqa_video_save(FILE *spool, char const *path)
{
    FILE *out;
    unsigned char block[16384];
    size_t count;
    int ok;
    char message[1024];

    if (spool == NULL) { return SIXEL_OK; }
    if (fflush(spool) != 0 || fseek(spool, 0, SEEK_SET) != 0) {
        sixel_helper_set_additional_message("cannot read video output spool");
        return SIXEL_RUNTIME_ERROR;
    }
    out = fopen(path, "wb");
    if (out == NULL) {
        snprintf(message, sizeof(message), "cannot open video output: %s",
            path);
        sixel_helper_set_additional_message(message);
        return SIXEL_RUNTIME_ERROR;
    }
    ok = 1;
    while ((count = fread(block, 1, sizeof(block), spool)) != 0) {
        if (fwrite(block, 1, count, out) != count) { ok = 0; break; }
    }
    if (ferror(spool)) { ok = 0; }
    if (fclose(out) != 0) { ok = 0; }
    if (!ok) {
        sixel_helper_set_additional_message("cannot finish video output");
    }
    return ok ? SIXEL_OK : SIXEL_RUNTIME_ERROR;
}

static void
lsqa_video_summary(FILE *file, lsqa_video_state_t const *v,
    double const *means)
{
    fprintf(file, "{\"type\":\"summary\",\"complete\":true,"
        "\"alignment\":\"%s\",\"frames\":%zu,\"duration\":%.17g,"
        "\"bytes\":%zu,\"frame_bytes\":%zu,\"trailing_bytes\":%zu,"
        "\"packets\":%zu,\"bytes_per_second\":%.17g,"
        "\"aggregation\":\"duration-weighted mean\",\"quality\":",
        v->options->clock_alignment ? "clock" : "frame",
        v->frame, v->pts, v->input_size, v->reported_end,
        v->input_size - v->byte_end, v->packets, v->input_size / v->pts);
    lsqa_video_metrics(file, means, v->valid_mask);
    fputs(",\"temporal_min\":", file);
    lsqa_video_metrics(file, v->minima, v->valid_mask & v->finite_mask);
    fputs(",\"temporal_max\":", file);
    lsqa_video_metrics(file, v->maxima, v->valid_mask & v->finite_mask);
    if (v->options->clock_alignment) {
        fprintf(file, ",\"segments\":%zu,\"late_updates\":%zu,"
            "\"late_bytes\":%zu,\"initial_blank_duration\":%.17g,"
            "\"evaluation_end\":%.17g", v->samples, v->late_updates,
            v->byte_end - v->reported_end, v->blank_duration, v->pts);
    }
    if (v->report != NULL) {
        fprintf(file, ",\"persistence\":{\"weighted_excess\":%.17g,"
            "\"persistent_fraction\":%.17g,\"max_age\":%.17g}",
            v->persistence.weighted_excess,
            v->persistence.persistent_fraction, v->persistence.max_age);
    }
    fputs("}\n", file);
}

SIXEL_INTERNAL_API SIXELSTATUS
lsqa_video_run(lsqa_video_options_t const *options,
    sixel_allocator_t *allocator, double *means, unsigned int *valid_mask)
{
    lsqa_video_state_t v;
    sixel_chunk_t *chunk;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    unsigned char const *rgba;
    unsigned char *profile;
    FILE *icc;
    char const *profile_path;
    char const *spec;
    char const *cms;
    long profile_size;
    size_t end;
    double pts;
    double duration;
    int width;
    int height;
    int value;
    int i;
    unsigned int metrics;

    memset(&v, 0, sizeof(v));
    chunk = NULL;
    loader = NULL;
    profile = NULL;
    icc = NULL;
    v.options = options;
    v.allocator = allocator;
    *valid_mask = 0;
    if (!lsqa_video_distinct(options->report, options->reference)
            || !lsqa_video_distinct(options->report, options->target)
            || !lsqa_video_distinct(options->report, options->timeline)
            || !lsqa_video_distinct(options->report, options->display_timeline)
            || !lsqa_video_distinct(options->error_map, options->reference)
            || !lsqa_video_distinct(options->error_map, options->target)
            || !lsqa_video_distinct(options->error_map, options->timeline)
            || !lsqa_video_distinct(options->error_map, options->report)
            || !lsqa_video_distinct(options->error_map,
                options->display_timeline)) {
        return lsqa_video_error("report/map must be distinct named files");
    }
    spec = strcmp(options->target_colorspace, "auto") == 0
        ? "srgb" : options->target_colorspace;
    status = SIXEL_BAD_INPUT;
    if (!sixel_cdef_parse(spec, &v.definition, &profile_path, &v.intent)) {
        goto cleanup;
    }
    if (profile_path != NULL) {
        icc = fopen(profile_path, "rb");
        if (icc == NULL || fseek(icc, 0, SEEK_END) != 0) { goto cleanup; }
        profile_size = ftell(icc);
        if (profile_size < 128 || profile_size > 16 * 1024 * 1024
                || fseek(icc, 0, SEEK_SET) != 0) { goto cleanup; }
        profile = malloc((size_t)profile_size);
        if (profile == NULL) { status = SIXEL_BAD_ALLOCATION; goto cleanup; }
        if (fread(profile, 1, (size_t)profile_size, icc)
                != (size_t)profile_size) { goto cleanup; }
        v.definition = sixel_cdef_icc(profile, (size_t)profile_size);
    }
    v.srgb = sixel_cdef_name("srgb");
    if (v.definition == NULL || v.srgb == NULL) { goto cleanup; }
    status = sixel_chunk_create_from_source(&chunk, options->target,
        0, NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    v.input_size = sixel_chunk_get_size(chunk);
    status = sixel_sequence_open(&v.decoder, sixel_chunk_get_buffer(chunk),
        v.input_size, 1, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = SIXEL_RUNTIME_ERROR;
    if (options->timeline != NULL) {
        v.timeline = fopen(options->timeline, "rb");
        if (v.timeline == NULL) {
            sixel_helper_set_additional_message("cannot open target timeline");
            goto cleanup;
        }
    }
    if (options->display_timeline != NULL) {
        v.display = fopen(options->display_timeline, "rb");
        if (v.display == NULL) {
            sixel_helper_set_additional_message("cannot open display timeline");
            goto cleanup;
        }
        status = lsqa_video_event(&v);
        if (SIXEL_FAILED(status)) { goto cleanup; }
        if (!v.pending) {
            status = lsqa_video_error("empty display timeline");
            goto cleanup;
        }
    }
    status = SIXEL_RUNTIME_ERROR;
    if (options->report != NULL) {
        v.report = tmpfile();
        if (v.report == NULL) {
            sixel_helper_set_additional_message("cannot create report spool");
            goto cleanup;
        }
        fprintf(v.report, "{\"type\":\"metadata\",\"schema\":\"%s\","
            "\"alignment\":\"%s\",\"initial_palette\":\"libsixel-default\","
            "\"initial_register\":15,\"reference\":",
            options->clock_alignment ? "lsqa-clock-report-v1"
                : "lsqa-frame-report-v1",
            options->clock_alignment ? "clock" : "frame");
        lsqa_video_string(v.report, options->reference);
        fputs(",\"target\":", v.report);
        lsqa_video_string(v.report, options->target);
        fputs(",\"target_colorspace\":", v.report);
        lsqa_video_string(v.report, spec);
        fputs(",\"loaders\":", v.report);
        lsqa_video_string(v.report,
            options->loaders == NULL ? "auto" : options->loaders);
        cms = getenv("SIXEL_LOADER_CMS_ENGINE");
        fputs(",\"cms_engine\":", v.report);
        lsqa_video_string(v.report, cms == NULL ? "auto" : cms);
        fprintf(v.report, ",\"compare_colorspace\":\"%s\","
            "\"compare_precision\":\"%s\",\"grayscale\":%s,"
            "\"region_window\":%d,\"reference_fps\":%.17g,"
            "\"background_srgb\":[%u,%u,%u]",
            options->signal_space < 0 ? "reference"
                : sixel_assessment_colorspace_name(options->signal_space),
            options->signal_bytes < 0 ? "reference"
                : options->signal_bytes ? "8bit" : "float32",
            options->grayscale ? "true" : "false", options->region_window,
            options->reference_fps,
            options->background == NULL ? 0U : options->background[0],
            options->background == NULL ? 0U : options->background[1],
            options->background == NULL ? 0U : options->background[2]);
        if (options->timeline != NULL) {
            fputs(",\"timeline\":", v.report);
            lsqa_video_string(v.report, options->timeline);
        }
        if (options->display_timeline != NULL) {
            fputs(",\"display_timeline\":", v.report);
            lsqa_video_string(v.report, options->display_timeline);
            fputs(",\"clock_basis\":\"caller-supplied packet availability\","
                "\"evaluation_window\":\"one reference cycle [0,end)\","
                "\"initial_display\":\"background\"", v.report);
        }
        fprintf(v.report, ",\"persistence\":{"
            "\"model\":\"screen-exceedance-linear-ramp-v1\","
            "\"ramp_seconds\":%.17g,\"threshold\":%.17g,"
            "\"motion_compensated\":false}", options->persistence_time,
            options->threshold);
        fprintf(v.report, ",\"scielab_ppd\":%.17g,"
            "\"scielab_model\":\"zhang-wandell-1998-unit-dc\","
            "\"scielab_difference\":\"CIE76\","
            "\"scielab_boundary\":\"edge-repeating reflection\","
            "\"scielab_threshold\":%.17g,\"timing\":\"%s\","
            "\"mapping\":\"%s\"", options->ppd, options->threshold,
            options->timeline != NULL ? "external timeline"
                : options->reference_fps > 0 ? "reference-fps override"
                : "loader centiseconds",
            options->clock_alignment ? "display timeline intersections"
                : options->timeline != NULL ? "external timeline"
                : "one DCS per reference frame");
        if (options->error_map != NULL) {
            fputs(",\"error_map\":{\"path\":", v.report);
            lsqa_video_string(v.report, options->error_map);
            fputs(",\"format\":\"float32-le\",\"order\":\"row-major\","
                "\"units\":\"spatial CIE76\"}", v.report);
        }
        fputs("}\n", v.report);
    }
    if (options->error_map != NULL) {
        v.maps = tmpfile();
        if (v.maps == NULL) {
            sixel_helper_set_additional_message(
                "cannot create error-map spool");
            goto cleanup;
        }
    }
    status = sixel_assessment_new(&v.qa, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    v.qa->scielab_ppd = options->ppd;
    v.qa->region_window = options->region_window;
    v.qa->grayscale = options->grayscale;
    /* Scalar-only invocations keep the existing -m computation limit.
     * An explicit report requests the complementary spatial diagnostics. */
    metrics = options->metrics;
    if (v.report != NULL) {
        metrics |= SIXEL_ASSESSMENT_METRIC_MASK(SIXEL_ASSESSMENT_METRIC_SCIELAB)
            | SIXEL_ASSESSMENT_METRIC_MASK(SIXEL_ASSESSMENT_METRIC_SCIELAB_MAX)
            | SIXEL_ASSESSMENT_METRIC_MASK(SIXEL_ASSESSMENT_METRIC_MS_SSIM);
    }
    sixel_assessment_select_metrics(v.qa, metrics);
    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    value = 0;
    status = sixel_loader_setopt(loader,
        SIXEL_LOADER_OPTION_REQUIRE_STATIC, &value);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE,
        &value);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    value = SIXEL_LOOP_DISABLE;
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOOP_CONTROL,
        &value);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, &v);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    if (options->loaders != NULL) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
            options->loaders);
        if (SIXEL_FAILED(status)) { goto cleanup; }
    }
    if (options->background != NULL) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_BGCOLOR,
            options->background);
        if (SIXEL_FAILED(status)) { goto cleanup; }
    }
    status = sixel_loader_load_file(loader, options->reference,
        lsqa_video_frame);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    if (v.frame == 0 || (v.timeline != NULL
            && lsqa_video_timeline(v.timeline, v.frame, &end,
                &pts, &duration) != 0)) {
        status = lsqa_video_error("reference/timeline frame count mismatch");
        goto cleanup;
    }
    /* Validate the whole sidecar/capture, including updates at or beyond
     * the reference end. They cost bytes but never extend the score window
     * with an invented frozen reference or conceal delayed final output. */
    while (v.pending) {
        status = lsqa_video_pull(&v, v.next_end, 0);
        if (SIXEL_FAILED(status)) { goto cleanup; }
        ++v.late_updates;
        status = lsqa_video_event(&v);
        if (SIXEL_FAILED(status)) { goto cleanup; }
    }
    status = sixel_sequence_next(v.decoder, &rgba, &width, &height, &end);
    if (status != SIXEL_FALSE) {
        status = lsqa_video_error("extra or malformed target tail");
        goto cleanup;
    }
    for (i = 0; i < SIXEL_ASSESSMENT_METRIC_COUNT; ++i) {
        means[i] = v.averages[i];
    }
    if (v.report != NULL) {
        lsqa_video_summary(v.report, &v, means);
        if (ferror(v.report)) { status = SIXEL_RUNTIME_ERROR; goto cleanup; }
    }
    status = lsqa_video_save(v.maps, options->error_map);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = lsqa_video_save(v.report, options->report);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    if (options->print_summary) {
        lsqa_video_summary(stdout, &v, means);
        if (fflush(stdout) != 0) { status = SIXEL_RUNTIME_ERROR; goto cleanup; }
    }
    *valid_mask = v.valid_mask;
    status = SIXEL_OK;
cleanup:
    sixel_loader_unref(loader);
    sixel_assessment_unref(v.qa);
    sixel_sequence_close(v.decoder);
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    if (v.report != NULL) { fclose(v.report); }
    if (v.maps != NULL) { fclose(v.maps); }
    if (v.timeline != NULL) { fclose(v.timeline); }
    if (v.display != NULL) { fclose(v.display); }
    if (icc != NULL) { fclose(icc); }
    sixel_cdef_free(v.definition);
    sixel_cdef_free(v.srgb);
    free(profile);
    free(v.errors);
    free(v.scratch);
    free(v.ages);
    free(v.rgb);
    return status;
}
