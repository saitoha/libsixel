/* SPDX-License-Identifier: MIT */
/* Independent integration and event-boundary assertions for video reports. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/compat_stub.h"

static double
clock_report_number(char const *line, char const *key)
{
    char token[64];
    char const *p;
    char *end;
    double value;

    snprintf(token, sizeof(token), "\"%s\":", key);
    p = strstr(line, token);
    if (p == NULL) { return NAN; }
    p += strlen(token);
    value = strtod(p, &end);
    return end != p && (*end == ',' || *end == '}') ? value : NAN;
}

/* Antiderivative of the ramp: a triangle, then a rectangle. This oracle
 * deliberately differs from the production normalized-interval formula. */
static double
clock_report_area(double age, double ramp)
{
    return ramp == 0 || age >= ramp ? age - ramp / 2 : age * age / (2 * ramp);
}

static int
clock_report_persistence(FILE *maps, char const *line, double *ages,
    double dt, double ramp)
{
    unsigned char bytes[4];
    uint32_t bits;
    unsigned int exponent;
    double error;
    double previous;
    double excess;
    double fraction;
    double maximum;
    int i;

    excess = fraction = maximum = 0;
    for (i = 0; i < 256; ++i) {
        if (fread(bytes, 1, 4, maps) != 4) { return 0; }
        bits = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8)
            | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
        exponent = (bits >> 23) & 255U;
        if (exponent == 255U || (bits >> 31) != 0) { return 0; }
        error = exponent != 0
            ? ldexp(1.0 + (bits & 0x7fffffU) / 8388608.0,
                (int)exponent - 127)
            : ldexp((double)(bits & 0x7fffffU), -149);
        if (error > 256) { return 0; }
        previous = ages[i];
        if (error <= 2) {
            ages[i] = 0;
            continue;
        }
        ages[i] += dt;
        excess += (error - 2) * (clock_report_area(ages[i], ramp)
            - clock_report_area(previous, ramp)) / (256 * dt);
        fraction += (fmax(ages[i] - ramp, 0) - fmax(previous - ramp, 0))
            / (256 * dt);
        maximum = fmax(maximum, ages[i]);
    }
    /* Float32 export of errors <= 256 contributes <= 8e-6 absolute error;
     * 1e-5 covers that rounding plus the independent integral arithmetic. */
    return fabs(excess - clock_report_number(line, "weighted_excess")) < 1e-5
        && fabs(fraction - clock_report_number(line, "persistent_fraction"))
            < 1e-12
        && fabs(maximum - clock_report_number(line, "max_age")) < 1e-12;
}

int
test_clock_report_check(int argc, char **argv)
{
    static double const starts[] = {0, 0.02, 0.04, 0.10, 0.12};
    static double const lengths[] = {0.02, 0.02, 0.06, 0.02, 0.12};
    static int const frames[] = {0, 0, 1, 1, 2};
    FILE *report;
    FILE *maps;
    char line[32768];
    char const *region;
    double ages[256];
    double dt;
    double elapsed;
    double weighted;
    double persistence;
    double coverage;
    double max_age;
    double mean;
    double value;
    double ramp;
    int mode;
    int samples;
    int headers;
    int summaries;
    int ok;
    int byte;
    int count;

    if (argc < 3) { return EXIT_FAILURE; }
    mode = atoi(argv[1]);
    report = sixel_compat_fopen(argv[2], "rb");
    if (report == NULL) { return EXIT_FAILURE; }
    maps = argc == 4 ? sixel_compat_fopen(argv[3], "rb") : NULL;
    ok = argc != 4 || maps != NULL;
    samples = headers = summaries = 0;
    elapsed = weighted = persistence = coverage = max_age = 0;
    ramp = mode == 7 ? 0 : 0.1;
    memset(ages, 0, sizeof(ages));
    while (ok && fgets(line, sizeof(line), report) != NULL) {
        if (strstr(line, "\"type\":\"metadata\"") != NULL) {
            ++headers;
            ok = clock_report_number(line, "ramp_seconds") == ramp
                && strstr(line, "\"motion_compensated\":false") != NULL;
            if (mode < 6) {
                ok = ok && strstr(line, "lsqa-clock-report-v1") != NULL;
            }
        } else if (strstr(line, mode < 6 ? "\"type\":\"segment\""
                : "\"type\":\"frame\"") != NULL) {
            mean = clock_report_number(line, "mean");
            dt = clock_report_number(line, "duration");
            ok = isfinite(mean) && isfinite(dt) && dt > 0
                && fabs(clock_report_number(line, "pts") - elapsed) < 1e-12;
            if (!ok) { break; }
            elapsed += dt;
            weighted += mean * dt;
            value = clock_report_number(line, "weighted_excess");
            ok = isfinite(value) && value >= 0;
            persistence += value * dt;
            coverage += clock_report_number(line, "persistent_fraction") * dt;
            max_age = fmax(max_age, clock_report_number(line, "max_age"));
            if (mode == 1) {
                ok = ok && samples < 5
                    && clock_report_number(line, "segment") == samples
                    && clock_report_number(line, "frame") == frames[samples]
                    && fabs(clock_report_number(line, "pts")
                        - starts[samples]) < 1e-12
                    && fabs(dt - lengths[samples]) < 1e-12;
            } else if (mode == 2 && samples < 2) {
                ok = ok && (samples == 0 ? mean > 50 : mean < 1)
                    && clock_report_number(line, "byte_end")
                        == (samples == 0 ? 0 : 3762);
            } else if (mode == 4) {
                ok = ok && samples < 3
                    && clock_report_number(line, "frame") == samples
                    && clock_report_number(line, "packets")
                        == (samples == 0 ? 1 : samples == 1 ? 2 : 0)
                    && mean < 1;
            }
            if (mode == 5 || mode >= 6) {
                region = strstr(line, "\"error_map\":");
                ok = ok && region != NULL
                    && clock_report_number(region, "offset") == samples * 1024
                    && clock_report_number(region, "bytes") == 1024;
            }
            if (ok && mode >= 6) {
                ok = maps != NULL
                    && clock_report_persistence(maps, line, ages, dt, ramp);
            }
            ++samples;
        } else if (strstr(line, "\"type\":\"summary\"") != NULL) {
            ++summaries;
            ok = clock_report_number(line, "frames") == 3
                && fabs(clock_report_number(line, "duration") - 0.24) < 1e-12
                && fabs(clock_report_number(line, "S-CIELAB")
                    - weighted / elapsed) < 1e-12
                && fabs(clock_report_number(line, "weighted_excess")
                    - persistence / elapsed) < 1e-12
                && fabs(clock_report_number(line, "persistent_fraction")
                    - coverage / elapsed) < 1e-12
                && fabs(clock_report_number(line, "max_age") - max_age) < 1e-12;
            if (mode == 1) {
                ok = ok && clock_report_number(line, "S-CIELAB") > 10;
            } else if (mode == 2) {
                ok = ok && clock_report_number(line, "initial_blank_duration")
                    == 0.02;
            } else if (mode == 3) {
                ok = ok && clock_report_number(line, "late_updates") == 1
                    && clock_report_number(line, "late_bytes") == 3768
                    && clock_report_number(line, "bytes") == 11298
                    && clock_report_number(line, "frame_bytes") == 7530
                    && clock_report_number(line, "trailing_bytes") == 0;
            } else if (mode == 4) {
                ok = ok && clock_report_number(line, "late_updates") == 0;
            }
        } else {
            ok = 0;
        }
    }
    if (maps != NULL && mode == 5) {
        count = 0;
        while ((byte = fgetc(maps)) != EOF) { ++count; }
        if (count != 5120 || ferror(maps)) { ok = 0; }
    } else if (maps != NULL && (fgetc(maps) != EOF || ferror(maps))) {
        ok = 0;
    }
    if (ferror(report)) { ok = 0; }
    fclose(report);
    if (maps != NULL) { fclose(maps); }
    return ok && headers == 1 && summaries == 1
        && samples == (mode == 4 || mode >= 6 ? 3 : 5)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
