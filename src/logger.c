/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <pthread.h>
#endif

#include "logger.h"
#include "timer.h"
#include "compat_stub.h"

static sixel_logger_t *sixel_logger_active;
static int sixel_logger_refcount;

static unsigned long long
sixel_logger_thread_id(void)
{
#ifndef _WIN32
    return (unsigned long long)(uintptr_t)pthread_self();
#else
    return 0ULL;
#endif
}

void
sixel_logger_init(sixel_logger_t *logger)
{
    if (logger == NULL) {
        return;
    }
    logger->delegate = NULL;
    logger->file = NULL;
    logger->mutex_ready = 0;
    logger->active = 0;
    logger->started_at = 0.0;
}

void
sixel_logger_close(sixel_logger_t *logger)
{
    if (logger == NULL) {
        return;
    }
    if (logger->delegate != NULL) {
        if (logger->delegate == sixel_logger_active &&
                sixel_logger_refcount > 0) {
            --sixel_logger_refcount;
            if (sixel_logger_refcount != 0) {
                logger->delegate = NULL;
                logger->active = 0;
                return;
            }
            logger = logger->delegate;
            logger->delegate = NULL;
        } else {
            logger->delegate = NULL;
            logger->active = 0;
            return;
        }
    }
    if (logger == sixel_logger_active && sixel_logger_refcount > 1) {
        --sixel_logger_refcount;
        return;
    }
    if (logger->mutex_ready) {
#if SIXEL_ENABLE_THREADS
        sixel_mutex_destroy(&logger->mutex);
#endif  /* SIXEL_ENABLE_THREADS */
        logger->mutex_ready = 0;
    }
    if (logger->file != NULL) {
        fclose(logger->file);
        logger->file = NULL;
    }
    if (logger == sixel_logger_active && sixel_logger_refcount > 0) {
        sixel_logger_refcount = 0;
        sixel_logger_active = NULL;
    }
    logger->active = 0;
}

SIXELSTATUS
sixel_logger_open(sixel_logger_t *logger, char const *path)
{
    if (logger == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (path == NULL || path[0] == '\0') {
        return SIXEL_OK;
    }
    logger->file = sixel_compat_fopen(path, "w");
    if (logger->file == NULL) {
        return SIXEL_RUNTIME_ERROR;
    }
#if SIXEL_ENABLE_THREADS
    if (sixel_mutex_init(&logger->mutex) != 0) {
        fclose(logger->file);
        logger->file = NULL;
        return SIXEL_RUNTIME_ERROR;
    }
#endif  /* SIXEL_ENABLE_THREADS */
    logger->mutex_ready = 1;
    logger->active = 1;
    logger->started_at = sixel_timer_now();
    /*
     * Use fully buffered output to avoid newline-triggered flushes.  VPTE
     * timeline logging can emit many events, and line buffering would force
     * frequent kernel writes even without explicit fflush() calls.
     *
     * Some CRT implementations reject _IOFBF when the buffer size is zero.
     * Request a portable default size and keep logging active even if the
     * buffering hint is ignored.
     */
    (void)setvbuf(logger->file, NULL, _IOFBF, BUFSIZ);
    sixel_logger_active = logger;
    sixel_logger_refcount = 1;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_logger_prepare_env(sixel_logger_t *logger)
{
    char const *path;
    SIXELSTATUS status;

    if (logger == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * Reuse an already opened logger so the timeline is continuous.  Some
     * call sites share a logger instance across serial and pipeline phases;
     * reopening would truncate the file and reset timestamps, hiding early
     * stages from the timeline. Share the sink via a lightweight delegate
     * pointer instead of copying mutex state.
     */
    if (sixel_logger_active != NULL &&
            sixel_logger_active->active &&
            sixel_logger_active->mutex_ready) {
        sixel_logger_init(logger);
        logger->delegate = sixel_logger_active;
        logger->active = sixel_logger_active->active;
        logger->started_at = sixel_logger_active->started_at;
        ++sixel_logger_refcount;
        return SIXEL_OK;
    }

    sixel_logger_init(logger);
    path = sixel_compat_getenv("SIXEL_LOG_PATH");
    if (path == NULL || path[0] == '\0') {
        return SIXEL_OK;
    }
    status = sixel_logger_open(logger, path);
    if (SIXEL_FAILED(status)) {
        sixel_logger_close(logger);
    }

    return status;
}

void
sixel_logger_logf(sixel_logger_t *logger,
                           char const *role,
                           char const *worker,
                           char const *event,
                           int job_id,
                           ...)
{
    sixel_logger_t *target;
    double timestamp;

    /*
     * Extra arguments are accepted for compatibility with older call sites
     * but intentionally ignored to keep timeline logging lightweight.
     */
    target = logger;
    if (logger != NULL && logger->delegate != NULL) {
        target = logger->delegate;
    }
    if (target == NULL || !target->active || target->file == NULL
            || !target->mutex_ready) {
        return;
    }

#if SIXEL_ENABLE_THREADS
    sixel_mutex_lock(&target->mutex);
#endif  /* SIXEL_ENABLE_THREADS */
    timestamp = sixel_timer_now() - target->started_at;
    if (timestamp < 0.0) {
        timestamp = 0.0;
    }
    fprintf(target->file,
            "{\"ts\":%.6f,\"thread\":%llu,\"worker\":\"%s\","\
            "\"role\":\"%s\",\"event\":\"%s\",\"job\":%d}\n",
            timestamp,
            sixel_logger_thread_id(),
            worker != NULL ? worker : "",
            role != NULL ? role : "",
            event != NULL ? event : "",
            job_id);
    fflush(target->file);
#if SIXEL_ENABLE_THREADS
    sixel_mutex_unlock(&target->mutex);
#endif  /* SIXEL_ENABLE_THREADS */
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
