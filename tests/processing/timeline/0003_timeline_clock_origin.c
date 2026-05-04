/*
 * SPDX-License-Identifier: MIT
 *
 * Regression test for timeline clock ownership.  A logger created after the
 * writer clock starts must still report timestamps on the shared writer
 * timeline, not relative to its own construction time.  The C side only writes
 * and flushes the JSONL; the TAP script verifies the completed file using shell
 * builtins so MSVC does not reopen the just-flushed file inside the native
 * process.
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
    int success;

    allocator = NULL;
    writer = NULL;
    logger1 = NULL;
    logger2 = NULL;
    service = NULL;
    log_path_env = NULL;
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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
