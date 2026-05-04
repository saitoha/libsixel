/*
 * SPDX-License-Identifier: MIT
 *
 * Regression test for timeline clock ownership.  A logger created after the
 * writer clock starts must still report timestamps on the shared writer
 * timeline, not relative to its own construction time.  The test reads the
 * JSONL after an explicit writer flush so Windows process handoff timing
 * cannot hide the clock contract.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

#include "src/compat_stub.h"
#include "src/timer.h"

#define TIMELINE_CLOCK_WAIT_SECONDS 0.020
#define TIMELINE_CLOCK_MIN_DELTA_SECONDS 0.010

static void
timeline_wait_seconds(double seconds)
{
    double started_at;
    double now;

    started_at = sixel_timer_now();
    now = started_at;
    while (now - started_at < seconds) {
        now = sixel_timer_now();
        if (now <= 0.0 && started_at <= 0.0) {
            break;
        }
    }
}

static int
timeline_parse_timestamp(char const *line, double *timestamp)
{
    char const *field;
    char *endptr;
    double value;

    if (line == NULL || timestamp == NULL) {
        return 0;
    }
    field = strstr(line, "\"ts\":");
    if (field == NULL) {
        return 0;
    }
    field += strlen("\"ts\":");
    value = strtod(field, &endptr);
    if (endptr == (char *)field || value < 0.0) {
        return 0;
    }
    *timestamp = value;
    return 1;
}

static int
timeline_read_clock_samples(char const *path,
                            double *first_timestamp,
                            double *second_timestamp)
{
    FILE *file;
    char line[1024];
    int found_first;
    int found_second;

    if (path == NULL || path[0] == '\0' ||
        first_timestamp == NULL || second_timestamp == NULL) {
        return 0;
    }

    file = sixel_compat_fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "timeline clock log was not written: %s\n", path);
        return 0;
    }

    found_first = 0;
    found_second = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, "\"event\":\"first\"") != NULL) {
            if (!timeline_parse_timestamp(line, first_timestamp)) {
                fclose(file);
                return 0;
            }
            found_first = 1;
        }
        if (strstr(line, "\"event\":\"second\"") != NULL) {
            if (!timeline_parse_timestamp(line, second_timestamp)) {
                fclose(file);
                return 0;
            }
            found_second = 1;
        }
    }
    fclose(file);

    return found_first && found_second;
}

static int
timeline_clock_origin_is_shared(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_timeline_writer_t *writer;
    sixel_timeline_logger_t *logger1;
    sixel_timeline_logger_t *logger2;
    sixel_timeline_event_t event;
    void *service;
    char log_path[4096];
    char const *log_path_env;
    double first_timestamp;
    double second_timestamp;
    int success;

    allocator = NULL;
    writer = NULL;
    logger1 = NULL;
    logger2 = NULL;
    service = NULL;
    log_path_env = NULL;
    first_timestamp = 0.0;
    second_timestamp = 0.0;
    memset(log_path, 0, sizeof(log_path));
    success = 0;

    /*
     * MSVC compat getenv refreshes the cached pointer when the writer reads
     * the same variable.  Copy the test-owned path before creating loggers.
     */
    log_path_env = sixel_compat_getenv("SIXEL_LOG_PATH");
    if (log_path_env != NULL && log_path_env[0] != '\0' &&
        sixel_compat_strcpy(log_path, sizeof(log_path), log_path_env) < 0) {
        goto end;
    }
    if (log_path[0] == '\0') {
        goto end;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_components_getservice("services/timeline-writer",
                                         &service);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    writer = (sixel_timeline_writer_t *)service;
    service = NULL;
    if (writer == NULL || writer->vtbl == NULL ||
        writer->vtbl->create_logger == NULL ||
        writer->vtbl->flush == NULL ||
        writer->vtbl->unref == NULL) {
        goto end;
    }

    status = writer->vtbl->create_logger(writer, allocator, &logger1);
    if (SIXEL_FAILED(status) || logger1 == NULL ||
        logger1->vtbl == NULL || logger1->vtbl->log == NULL ||
        logger1->vtbl->unref == NULL) {
        goto end;
    }

    event.role = "timeline/clock";
    event.worker = "test";
    event.event = "first";
    event.job_id = 1;
    status = logger1->vtbl->log(logger1, &event);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    timeline_wait_seconds(TIMELINE_CLOCK_WAIT_SECONDS);

    status = writer->vtbl->create_logger(writer, allocator, &logger2);
    if (SIXEL_FAILED(status) || logger2 == NULL ||
        logger2->vtbl == NULL || logger2->vtbl->log == NULL ||
        logger2->vtbl->unref == NULL) {
        goto end;
    }

    event.event = "second";
    event.job_id = 2;
    status = logger2->vtbl->log(logger2, &event);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    writer->vtbl->flush(writer);

    if (!timeline_read_clock_samples(log_path,
                                     &first_timestamp,
                                     &second_timestamp)) {
        goto end;
    }
    if (second_timestamp - first_timestamp <
        TIMELINE_CLOCK_MIN_DELTA_SECONDS) {
        fprintf(stderr,
                "timeline clock origin regressed: first=%f second=%f\n",
                first_timestamp,
                second_timestamp);
        goto end;
    }

    success = 1;

end:
    if (logger2 != NULL && logger2->vtbl != NULL &&
        logger2->vtbl->unref != NULL) {
        logger2->vtbl->unref(logger2);
    }
    if (logger1 != NULL && logger1->vtbl != NULL &&
        logger1->vtbl->unref != NULL) {
        logger1->vtbl->unref(logger1);
    }
    if (writer != NULL && writer->vtbl != NULL &&
        writer->vtbl->unref != NULL) {
        writer->vtbl->unref(writer);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return success;
}

int
test_timeline_0003_timeline_clock_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!timeline_clock_origin_is_shared()) {
        fprintf(stderr, "timeline clock origin contract failed\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int
test_timeline_0003_timeline_clock_origin_verify(int argc, char **argv)
{
    char log_path[4096];
    char const *log_path_source;
    double first_timestamp;
    double second_timestamp;

    log_path_source = NULL;
    first_timestamp = 0.0;
    second_timestamp = 0.0;
    memset(log_path, 0, sizeof(log_path));
    if (argc >= 2 && argv != NULL && argv[1] != NULL &&
        argv[1][0] != '\0') {
        log_path_source = argv[1];
    } else {
        /*
         * Keep the fallback for direct developer runs and artifact triage.
         * The TAP test verifies in the producer process so MSVC shell/native
         * process handoff does not decide CI success.
         */
        log_path_source = sixel_compat_getenv("SIXEL_LOG_PATH");
    }
    if (log_path_source == NULL || log_path_source[0] == '\0' ||
        sixel_compat_strcpy(log_path,
                            sizeof(log_path),
                            log_path_source) < 0 ||
        !timeline_read_clock_samples(log_path,
                                     &first_timestamp,
                                     &second_timestamp)) {
        fprintf(stderr, "timeline clock JSONL verification failed\n");
        return EXIT_FAILURE;
    }
    if (second_timestamp - first_timestamp <
        TIMELINE_CLOCK_MIN_DELTA_SECONDS) {
        fprintf(stderr,
                "timeline clock origin regressed: first=%f second=%f\n",
                first_timestamp,
                second_timestamp);
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
