/* SPDX-License-Identifier: MIT */
/* Independent report assertions for the three-frame 16x16 snake fixture.
 * This test helper does not reuse production JSON or percentile routines. */
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
video_report_test_number(char const *line, char const *key)
{
    char token[64];
    char const *start;
    char *end;
    double value;

    snprintf(token, sizeof(token), "\"%s\":", key);
    start = strstr(line, token);
    if (start == NULL) { return NAN; }
    start += strlen(token);
    value = strtod(start, &end);
    return end != start && (*end == ',' || *end == '}') ? value : NAN;
}

static int
video_report_test_compare(void const *a, void const *b)
{
    double x;
    double y;

    x = *(double const *)a;
    y = *(double const *)b;
    return (x > y) - (x < y);
}

static int
video_report_test_map(FILE *file, char const *line)
{
    unsigned char bytes[4];
    uint32_t bits;
    unsigned int exponent;
    double values[256];
    double mean;
    int above;
    int i;

    mean = 0;
    above = 0;
    for (i = 0; i < 256; ++i) {
        if (fread(bytes, 1, 4, file) != 4) { return 0; }
        bits = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8)
            | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
        exponent = (bits >> 23) & 255U;
        if (exponent == 255U || (bits >> 31) != 0) { return 0; }
        values[i] = exponent != 0
            ? ldexp(1.0 + (bits & 0x7fffffU) / 8388608.0,
                (int)exponent - 127)
            : ldexp((double)(bits & 0x7fffffU), -149);
        if (values[i] > 10) { return 0; }
        mean += values[i] / 256.0;
        if (values[i] > 2) { ++above; }
    }
    qsort(values, 256, sizeof(values[0]), video_report_test_compare);
    /* Errors below 10 round by less than 6e-7 in float32. The 1e-6
     * allowance also covers text conversion and this independent sum. */
    return fabs(mean - video_report_test_number(line, "mean")) < 1e-6
        && fabs(values[255] - video_report_test_number(line, "max")) < 1e-6
        && fabs(values[243] - video_report_test_number(line, "p95")) < 1e-6
        && above == video_report_test_number(line, "above_count");
}

int
test_video_report_check(int argc, char **argv)
{
    FILE *report;
    FILE *maps;
    char line[32768];
    char const *region;
    int mode;
    int frames;
    int metadata;
    int summaries;
    int ok;
    double bytes;
    double duration;
    double weighted;
    double plain;
    double previous;
    double mean;
    double maximum;
    double p95;
    double above;
    double fraction;
    double dt;
    double count;
    double total;
    double score;
    int byte;

    if (argc < 3) { return EXIT_FAILURE; }
    mode = atoi(argv[1]);
    report = sixel_compat_fopen(argv[2], "rb");
    if (report == NULL) { return EXIT_FAILURE; }
    maps = argc == 4 ? sixel_compat_fopen(argv[3], "rb") : NULL;
    ok = argc != 4 || maps != NULL;
    frames = 0;
    metadata = 0;
    summaries = 0;
    bytes = 0;
    duration = 0;
    weighted = 0;
    plain = 0;
    previous = 0;
    while (ok && fgets(line, sizeof(line), report) != NULL) {
        if (strstr(line, "\"type\":\"metadata\"") != NULL) {
            ++metadata;
        } else if (strstr(line, "\"type\":\"frame\"") != NULL) {
            mean = video_report_test_number(line, "mean");
            maximum = video_report_test_number(line, "max");
            p95 = video_report_test_number(line, "p95");
            above = video_report_test_number(line, "above_count");
            fraction = video_report_test_number(line, "above_fraction");
            dt = video_report_test_number(line, "duration");
            count = video_report_test_number(line, "bytes");
            if (!isfinite(mean) || !isfinite(maximum) || !isfinite(p95)
                    || !isfinite(above) || !isfinite(fraction)
                    || !isfinite(dt) || dt <= 0 || !isfinite(count)
                    || count < 0 || mean < 0 || p95 < 0
                    || p95 > maximum || mean > maximum + 1e-9
                    || fraction != above / 256.0
                    || video_report_test_number(line, "frame") != frames
                    || video_report_test_number(line, "width") != 16
                    || video_report_test_number(line, "height") != 16) {
                ok = 0;
                break;
            }
            bytes += count;
            duration += dt;
            weighted += mean * dt;
            plain += mean;
            if (mode == 2 && frames == 2) {
                ok = count == 0 && mean == previous
                    && video_report_test_number(line, "packets") == 0;
            }
            if (mode == 3 && frames == 0) {
                ok = video_report_test_number(line, "packets") == 2;
            }
            if (mode == 9) {
                ok = fabs(dt - 1001.0 / 30000.0) < 1e-12;
            }
            if (mode == 10) {
                ok = video_report_test_number(line, "threshold") == 0
                    && fraction == 1;
            }
            if (mode == 5 || mode == 14) {
                region = strstr(line, "\"error_map\":");
                ok = region != NULL
                    && video_report_test_number(region, "offset")
                        == frames * 1024
                    && video_report_test_number(region, "bytes") == 1024;
                if (ok && mode == 14) {
                    ok = maps != NULL && video_report_test_map(maps, line);
                }
            }
            previous = mean;
            ++frames;
        } else if (strstr(line, "\"type\":\"summary\"") != NULL) {
            ++summaries;
            total = video_report_test_number(line, "bytes");
            ok = isfinite(total) && total == bytes
                && strstr(line, "\"complete\":true") != NULL;
            if (ok && mode == 4) {
                score = video_report_test_number(line, "S-CIELAB");
                ok = isfinite(score) && fabs(score - weighted / duration)
                    < 1e-6 && score - plain / frames > 1;
            }
        } else {
            ok = 0;
        }
    }
    if (maps != NULL && mode == 5) {
        count = 0;
        while ((byte = fgetc(maps)) != EOF) { ++count; }
        if (count != 3072 || ferror(maps)) { ok = 0; }
    } else if (maps != NULL && (fgetc(maps) != EOF || ferror(maps))) {
        ok = 0;
    }
    if (ferror(report)) { ok = 0; }
    fclose(report);
    if (maps != NULL) { fclose(maps); }
    return ok && frames == 3 && metadata == 1 && summaries == 1
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
