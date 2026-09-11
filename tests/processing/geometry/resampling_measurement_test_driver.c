/*
 * SPDX-License-Identifier: MIT
 *
 * Recompute resampling summary metrics from the recorded CSV samples and
 * compare them with the independent tolerance table. The accepted files use
 * simple unquoted fields, so a small local splitter is sufficient and avoids
 * making the shell TAP wrappers depend on awk or Python.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compat_stub.h"

#define RMB_METHOD_COUNT 10
#define RMB_SCENARIO_COUNT 2
#define RMB_BASELINE_MAX 80
#define RMB_FIELD_MAX 16
#define RMB_LINE_MAX 1024

typedef enum rmb_measurement {
    RMB_STENCIL,
    RMB_FREQUENCY,
    RMB_ISOTROPY,
    RMB_MOIRE,
    RMB_QUALITY,
    RMB_SPEED,
    RMB_MEASUREMENT_INVALID
} rmb_measurement_t;

typedef enum rmb_metric {
    RMB_NONZERO_COUNT,
    RMB_RESPONSE_SUM,
    RMB_RESPONSE_ABS_SUM,
    RMB_GAIN_NEAR_045,
    RMB_ALIAS_RMS,
    RMB_DIRECTIONAL_RANGE_OVER_MEAN,
    RMB_MOIRE_RMS,
    RMB_MS_SSIM,
    RMB_DELTA_E00_MEAN,
    RMB_SIXEL_BYTES,
    RMB_MEDIAN_RATIO_TO_NEAREST,
    RMB_METRIC_INVALID
} rmb_metric_t;

typedef struct rmb_baseline {
    int scenario;
    int method;
    rmb_metric_t metric;
    double expected;
    double tolerance;
} rmb_baseline_t;

typedef struct rmb_accumulator {
    double response_sum;
    double response_abs_sum;
    double nearest_distance;
    double near_gain;
    double alias_square_sum;
    double isotropy_sum;
    double isotropy_minimum;
    double isotropy_maximum;
    double moire_rms;
    double ms_ssim;
    double delta_e00_mean;
    double sixel_bytes;
    double median_ms;
    int nonzero_count;
    int alias_count;
    int isotropy_count;
    int moire_count;
    int quality_count;
    int speed_count;
} rmb_accumulator_t;

static char const *const rmb_method_names[RMB_METHOD_COUNT] = {
    "nearest",
    "gaussian",
    "hanning",
    "hamming",
    "bilinear",
    "welsh",
    "bicubic",
    "lanczos2",
    "lanczos3",
    "lanczos4"
};

static int
rmb_split(char *line, char *fields[RMB_FIELD_MAX])
{
    char *cursor;
    int field_count;

    cursor = line;
    field_count = 1;
    fields[0] = line;
    while (*cursor != '\0') {
        if (*cursor == ',' && field_count < RMB_FIELD_MAX) {
            *cursor = '\0';
            fields[field_count] = cursor + 1;
            ++field_count;
        } else if (*cursor == '\n' || *cursor == '\r') {
            *cursor = '\0';
            break;
        }
        ++cursor;
    }
    return field_count;
}

static int
rmb_method_index(char const *name)
{
    int index;

    for (index = 0; index < RMB_METHOD_COUNT; ++index) {
        if (strcmp(name, rmb_method_names[index]) == 0) {
            return index;
        }
    }
    return -1;
}

static int
rmb_scenario_index(char const *name)
{
    if (strcmp(name, "enlarge-4x") == 0) {
        return 0;
    }
    if (strcmp(name, "reduce-4x") == 0) {
        return 1;
    }
    if (name[0] == '\0') {
        return -1;
    }
    return -2;
}

static rmb_measurement_t
rmb_measurement_value(char const *name)
{
    if (strcmp(name, "stencil") == 0) {
        return RMB_STENCIL;
    }
    if (strcmp(name, "frequency") == 0) {
        return RMB_FREQUENCY;
    }
    if (strcmp(name, "isotropy") == 0) {
        return RMB_ISOTROPY;
    }
    if (strcmp(name, "moire") == 0) {
        return RMB_MOIRE;
    }
    if (strcmp(name, "quality") == 0) {
        return RMB_QUALITY;
    }
    if (strcmp(name, "speed") == 0) {
        return RMB_SPEED;
    }
    return RMB_MEASUREMENT_INVALID;
}

static rmb_metric_t
rmb_metric_value(char const *name)
{
    if (strcmp(name, "nonzero_count") == 0) {
        return RMB_NONZERO_COUNT;
    }
    if (strcmp(name, "response_sum") == 0) {
        return RMB_RESPONSE_SUM;
    }
    if (strcmp(name, "response_abs_sum") == 0) {
        return RMB_RESPONSE_ABS_SUM;
    }
    if (strcmp(name, "gain_near_0_45") == 0) {
        return RMB_GAIN_NEAR_045;
    }
    if (strcmp(name, "alias_rms") == 0) {
        return RMB_ALIAS_RMS;
    }
    if (strcmp(name, "directional_range_over_mean") == 0) {
        return RMB_DIRECTIONAL_RANGE_OVER_MEAN;
    }
    if (strcmp(name, "rms_error_vs_area_reference") == 0) {
        return RMB_MOIRE_RMS;
    }
    if (strcmp(name, "ms_ssim") == 0) {
        return RMB_MS_SSIM;
    }
    if (strcmp(name, "delta_e00_mean") == 0) {
        return RMB_DELTA_E00_MEAN;
    }
    if (strcmp(name, "sixel_bytes") == 0) {
        return RMB_SIXEL_BYTES;
    }
    if (strcmp(name, "median_ratio_to_nearest") == 0) {
        return RMB_MEDIAN_RATIO_TO_NEAREST;
    }
    return RMB_METRIC_INVALID;
}

static int
rmb_expected_baseline_count(rmb_measurement_t measurement)
{
    switch (measurement) {
    case RMB_STENCIL:
        return RMB_SCENARIO_COUNT * RMB_METHOD_COUNT * 3;
    case RMB_FREQUENCY:
        return RMB_METHOD_COUNT * 2;
    case RMB_ISOTROPY:
    case RMB_MOIRE:
        return RMB_METHOD_COUNT;
    case RMB_QUALITY:
        return RMB_METHOD_COUNT * 3;
    case RMB_SPEED:
        return RMB_METHOD_COUNT;
    default:
        return 0;
    }
}

static int
rmb_load_baselines(char const *path,
                   rmb_measurement_t measurement,
                   rmb_baseline_t baselines[RMB_BASELINE_MAX],
                   int *baseline_count)
{
    FILE *stream;
    char line[RMB_LINE_MAX];
    char *fields[RMB_FIELD_MAX];
    rmb_measurement_t row_measurement;
    rmb_metric_t metric;
    int field_count;
    int method;
    int scenario;
    int count;
    int duplicate;
    int index;

    stream = sixel_compat_fopen(path, "rb");
    if (stream == NULL) {
        fprintf(stderr, "cannot open numeric baseline: %s\n", path);
        return 0;
    }
    count = 0;
    (void)fgets(line, sizeof(line), stream);
    while (fgets(line, sizeof(line), stream) != NULL) {
        field_count = rmb_split(line, fields);
        if (field_count != 6) {
            fprintf(stderr, "invalid numeric baseline row\n");
            fclose(stream);
            return 0;
        }
        row_measurement = rmb_measurement_value(fields[0]);
        if (row_measurement != measurement) {
            continue;
        }
        scenario = rmb_scenario_index(fields[1]);
        method = rmb_method_index(fields[2]);
        metric = rmb_metric_value(fields[3]);
        if (scenario < -1 || method < 0 || metric == RMB_METRIC_INVALID ||
            count >= RMB_BASELINE_MAX) {
            fprintf(stderr, "invalid numeric baseline key\n");
            fclose(stream);
            return 0;
        }
        duplicate = 0;
        for (index = 0; index < count; ++index) {
            if (baselines[index].scenario == scenario &&
                baselines[index].method == method &&
                baselines[index].metric == metric) {
                duplicate = 1;
            }
        }
        if (duplicate) {
            fprintf(stderr, "duplicate numeric baseline key\n");
            fclose(stream);
            return 0;
        }
        baselines[count].scenario = scenario;
        baselines[count].method = method;
        baselines[count].metric = metric;
        baselines[count].expected = strtod(fields[4], NULL);
        baselines[count].tolerance = strtod(fields[5], NULL);
        if (!isfinite(baselines[count].expected) ||
            !isfinite(baselines[count].tolerance) ||
            baselines[count].tolerance < 0.0) {
            fprintf(stderr, "invalid numeric baseline value\n");
            fclose(stream);
            return 0;
        }
        ++count;
    }
    fclose(stream);
    if (count != rmb_expected_baseline_count(measurement)) {
        fprintf(stderr, "numeric baseline row count changed: %d\n", count);
        return 0;
    }
    *baseline_count = count;
    return 1;
}

static void
rmb_init_accumulators(
    rmb_accumulator_t values[RMB_SCENARIO_COUNT][RMB_METHOD_COUNT])
{
    int scenario;
    int method;

    memset(values, 0, sizeof(rmb_accumulator_t) *
           RMB_SCENARIO_COUNT * RMB_METHOD_COUNT);
    for (scenario = 0; scenario < RMB_SCENARIO_COUNT; ++scenario) {
        for (method = 0; method < RMB_METHOD_COUNT; ++method) {
            values[scenario][method].nearest_distance = DBL_MAX;
            values[scenario][method].isotropy_minimum = DBL_MAX;
            values[scenario][method].isotropy_maximum = -DBL_MAX;
        }
    }
}

static int
rmb_read_stencils(
    FILE *stream,
    rmb_accumulator_t values[RMB_SCENARIO_COUNT][RMB_METHOD_COUNT])
{
    char line[RMB_LINE_MAX];
    char *fields[RMB_FIELD_MAX];
    double response;
    int field_count;
    int scenario;
    int method;

    while (fgets(line, sizeof(line), stream) != NULL) {
        field_count = rmb_split(line, fields);
        if (field_count != 4) {
            return 0;
        }
        scenario = rmb_scenario_index(fields[0]);
        method = rmb_method_index(fields[1]);
        if (scenario < 0 || method < 0) {
            return 0;
        }
        response = strtod(fields[3], NULL);
        if (response != 0.0) {
            ++values[scenario][method].nonzero_count;
            values[scenario][method].response_sum += response;
            values[scenario][method].response_abs_sum += fabs(response);
        }
    }
    return 1;
}

static int
rmb_read_frequency(
    FILE *stream,
    rmb_accumulator_t values[RMB_SCENARIO_COUNT][RMB_METHOD_COUNT])
{
    char line[RMB_LINE_MAX];
    char *fields[RMB_FIELD_MAX];
    rmb_accumulator_t *value;
    double frequency;
    double gain;
    double distance;
    int field_count;
    int method;

    while (fgets(line, sizeof(line), stream) != NULL) {
        field_count = rmb_split(line, fields);
        if (field_count != 4) {
            return 0;
        }
        method = rmb_method_index(fields[0]);
        if (method < 0) {
            return 0;
        }
        frequency = strtod(fields[1], NULL);
        gain = strtod(fields[2], NULL);
        value = &values[0][method];
        distance = fabs(frequency - 0.45);
        if (distance < value->nearest_distance) {
            value->nearest_distance = distance;
            value->near_gain = gain;
        }
        if (frequency > 0.5) {
            value->alias_square_sum += gain * gain;
            ++value->alias_count;
        }
    }
    return 1;
}

static int
rmb_read_isotropy(
    FILE *stream,
    rmb_accumulator_t values[RMB_SCENARIO_COUNT][RMB_METHOD_COUNT])
{
    char line[RMB_LINE_MAX];
    char *fields[RMB_FIELD_MAX];
    rmb_accumulator_t *value;
    double gain;
    int field_count;
    int method;

    while (fgets(line, sizeof(line), stream) != NULL) {
        field_count = rmb_split(line, fields);
        if (field_count != 5) {
            return 0;
        }
        method = rmb_method_index(fields[0]);
        if (method < 0) {
            return 0;
        }
        gain = strtod(fields[3], NULL);
        value = &values[0][method];
        value->isotropy_sum += gain;
        ++value->isotropy_count;
        if (gain < value->isotropy_minimum) {
            value->isotropy_minimum = gain;
        }
        if (gain > value->isotropy_maximum) {
            value->isotropy_maximum = gain;
        }
    }
    return 1;
}

static int
rmb_read_moire(
    FILE *stream,
    rmb_accumulator_t values[RMB_SCENARIO_COUNT][RMB_METHOD_COUNT])
{
    char line[RMB_LINE_MAX];
    char *fields[RMB_FIELD_MAX];
    int field_count;
    int method;

    while (fgets(line, sizeof(line), stream) != NULL) {
        field_count = rmb_split(line, fields);
        if (field_count != 3) {
            return 0;
        }
        method = rmb_method_index(fields[0]);
        if (method < 0) {
            return 0;
        }
        values[0][method].moire_rms = strtod(fields[1], NULL);
        ++values[0][method].moire_count;
    }
    return 1;
}

static int
rmb_read_quality(
    FILE *stream,
    rmb_accumulator_t values[RMB_SCENARIO_COUNT][RMB_METHOD_COUNT])
{
    char line[RMB_LINE_MAX];
    char *fields[RMB_FIELD_MAX];
    int field_count;
    int method;

    while (fgets(line, sizeof(line), stream) != NULL) {
        field_count = rmb_split(line, fields);
        if (field_count != 11) {
            return 0;
        }
        method = rmb_method_index(fields[0]);
        if (method < 0) {
            return 0;
        }
        values[0][method].ms_ssim = strtod(fields[1], NULL);
        values[0][method].delta_e00_mean = strtod(fields[2], NULL);
        values[0][method].sixel_bytes = strtod(fields[6], NULL);
        ++values[0][method].quality_count;
    }
    return 1;
}

static int
rmb_read_speed(
    FILE *stream,
    rmb_accumulator_t values[RMB_SCENARIO_COUNT][RMB_METHOD_COUNT])
{
    char line[RMB_LINE_MAX];
    char *fields[RMB_FIELD_MAX];
    int field_count;
    int method;

    while (fgets(line, sizeof(line), stream) != NULL) {
        field_count = rmb_split(line, fields);
        if (field_count != 8) {
            return 0;
        }
        method = rmb_method_index(fields[0]);
        if (method < 0) {
            return 0;
        }
        values[0][method].median_ms = strtod(fields[1], NULL);
        ++values[0][method].speed_count;
    }
    return 1;
}

static int
rmb_load_measurement(
    char const *path,
    rmb_measurement_t measurement,
    rmb_accumulator_t values[RMB_SCENARIO_COUNT][RMB_METHOD_COUNT])
{
    FILE *stream;
    char header[RMB_LINE_MAX];
    int valid;

    stream = sixel_compat_fopen(path, "rb");
    if (stream == NULL) {
        fprintf(stderr, "cannot open resampling measurement: %s\n", path);
        return 0;
    }
    if (fgets(header, sizeof(header), stream) == NULL) {
        fclose(stream);
        return 0;
    }
    switch (measurement) {
    case RMB_STENCIL:
        valid = rmb_read_stencils(stream, values);
        break;
    case RMB_FREQUENCY:
        valid = rmb_read_frequency(stream, values);
        break;
    case RMB_ISOTROPY:
        valid = rmb_read_isotropy(stream, values);
        break;
    case RMB_MOIRE:
        valid = rmb_read_moire(stream, values);
        break;
    case RMB_QUALITY:
        valid = rmb_read_quality(stream, values);
        break;
    case RMB_SPEED:
        valid = rmb_read_speed(stream, values);
        break;
    default:
        valid = 0;
        break;
    }
    fclose(stream);
    return valid;
}

static int
rmb_actual_value(
    rmb_baseline_t const *baseline,
    rmb_accumulator_t values[RMB_SCENARIO_COUNT][RMB_METHOD_COUNT],
    double *actual)
{
    rmb_accumulator_t *value;
    int scenario;

    scenario = baseline->scenario < 0 ? 0 : baseline->scenario;
    value = &values[scenario][baseline->method];
    switch (baseline->metric) {
    case RMB_NONZERO_COUNT:
        *actual = (double)value->nonzero_count;
        return 1;
    case RMB_RESPONSE_SUM:
        *actual = value->response_sum;
        return 1;
    case RMB_RESPONSE_ABS_SUM:
        *actual = value->response_abs_sum;
        return 1;
    case RMB_GAIN_NEAR_045:
        if (value->nearest_distance == DBL_MAX) {
            return 0;
        }
        *actual = value->near_gain;
        return 1;
    case RMB_ALIAS_RMS:
        if (value->alias_count <= 0) {
            return 0;
        }
        *actual = sqrt(value->alias_square_sum /
                       (double)value->alias_count);
        return 1;
    case RMB_DIRECTIONAL_RANGE_OVER_MEAN:
        if (value->isotropy_count <= 0 || value->isotropy_sum == 0.0) {
            return 0;
        }
        *actual = (value->isotropy_maximum -
                   value->isotropy_minimum) /
                  (value->isotropy_sum /
                   (double)value->isotropy_count);
        return 1;
    case RMB_MOIRE_RMS:
        if (value->moire_count != 1) {
            return 0;
        }
        *actual = value->moire_rms;
        return 1;
    case RMB_MS_SSIM:
        if (value->quality_count != 1) {
            return 0;
        }
        *actual = value->ms_ssim;
        return 1;
    case RMB_DELTA_E00_MEAN:
        if (value->quality_count != 1) {
            return 0;
        }
        *actual = value->delta_e00_mean;
        return 1;
    case RMB_SIXEL_BYTES:
        if (value->quality_count != 1) {
            return 0;
        }
        *actual = value->sixel_bytes;
        return 1;
    case RMB_MEDIAN_RATIO_TO_NEAREST:
        if (value->speed_count != 1 ||
            values[0][0].speed_count != 1 ||
            values[0][0].median_ms <= 0.0) {
            return 0;
        }
        *actual = value->median_ms / values[0][0].median_ms;
        return 1;
    default:
        return 0;
    }
}

static int
rmb_compare(
    rmb_baseline_t const baselines[RMB_BASELINE_MAX],
    int baseline_count,
    rmb_accumulator_t values[RMB_SCENARIO_COUNT][RMB_METHOD_COUNT])
{
    double actual;
    double difference;
    int index;

    for (index = 0; index < baseline_count; ++index) {
        if (!rmb_actual_value(&baselines[index], values, &actual)) {
            fprintf(stderr, "numeric measurement is incomplete\n");
            return 0;
        }
        difference = fabs(actual - baselines[index].expected);
        if (difference > baselines[index].tolerance) {
            fprintf(stderr,
                    "%s metric %d: %.8f differs from %.8f by %.8f\n",
                    rmb_method_names[baselines[index].method],
                    (int)baselines[index].metric,
                    actual,
                    baselines[index].expected,
                    difference);
            return 0;
        }
    }
    return 1;
}

int
test_geometry_resampling_measurement_baseline(int argc, char **argv)
{
    rmb_baseline_t baselines[RMB_BASELINE_MAX];
    rmb_accumulator_t
        values[RMB_SCENARIO_COUNT][RMB_METHOD_COUNT];
    rmb_measurement_t measurement;
    int baseline_count;

    baseline_count = 0;
    if (argc != 4) {
        fprintf(stderr, "usage: %s TYPE BASELINE.csv MEASUREMENT.csv\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    measurement = rmb_measurement_value(argv[1]);
    if (measurement == RMB_MEASUREMENT_INVALID) {
        fprintf(stderr, "unknown measurement type: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    rmb_init_accumulators(values);
    if (!rmb_load_baselines(argv[2],
                            measurement,
                            baselines,
                            &baseline_count) ||
        !rmb_load_measurement(argv[3], measurement, values) ||
        !rmb_compare(baselines, baseline_count, values)) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
