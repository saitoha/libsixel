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

#if defined(_WIN32)
#include <windows.h>
#else
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
#if defined(_WIN32)
    return (unsigned long long)GetCurrentThreadId();
#else
    return (unsigned long long)(uintptr_t)pthread_self();
#endif
}

static sixel_logger_t *
sixel_logger_resolve_target(sixel_logger_t *logger)
{
    if (logger == NULL) {
        return NULL;
    }
    if (logger->delegate != NULL) {
        return logger->delegate;
    }
    return logger;
}

static void
sixel_logger_clear_frame_context_slots(sixel_logger_t *logger)
{
    int index;

    if (logger == NULL) {
        return;
    }
    for (index = 0; index < SIXEL_LOGGER_FRAME_CONTEXT_SLOTS; ++index) {
        logger->frame_contexts[index].thread_id = 0ULL;
        logger->frame_contexts[index].frame_no = -1;
        logger->frame_contexts[index].loop_no = -1;
        logger->frame_contexts[index].multiframe = 0;
        logger->frame_contexts[index].active = 0;
    }
}

static int
sixel_logger_find_frame_context_slot_locked(sixel_logger_t *logger,
                                            unsigned long long thread_id,
                                            int create_if_missing)
{
    int free_index;
    int index;

    free_index = -1;
    if (logger == NULL) {
        return -1;
    }

    for (index = 0; index < SIXEL_LOGGER_FRAME_CONTEXT_SLOTS; ++index) {
        if (logger->frame_contexts[index].active != 0) {
            if (logger->frame_contexts[index].thread_id == thread_id) {
                return index;
            }
        } else if (free_index < 0) {
            free_index = index;
        }
    }

    if (create_if_missing == 0) {
        return -1;
    }
    if (free_index >= 0) {
        return free_index;
    }

    return (int)(thread_id
                 % (unsigned long long)SIXEL_LOGGER_FRAME_CONTEXT_SLOTS);
}

static void
sixel_logger_set_frame_context_locked(sixel_logger_t *logger,
                                      unsigned long long thread_id,
                                      int frame_no,
                                      int loop_no,
                                      int multiframe)
{
    int slot;

    slot = sixel_logger_find_frame_context_slot_locked(logger,
                                                       thread_id,
                                                       1);
    if (slot < 0) {
        return;
    }
    logger->frame_contexts[slot].thread_id = thread_id;
    logger->frame_contexts[slot].frame_no = frame_no;
    logger->frame_contexts[slot].loop_no = loop_no;
    logger->frame_contexts[slot].multiframe = multiframe;
    logger->frame_contexts[slot].active = 1;
}

static void
sixel_logger_clear_frame_context_locked(sixel_logger_t *logger,
                                        unsigned long long thread_id)
{
    int slot;

    slot = sixel_logger_find_frame_context_slot_locked(logger,
                                                       thread_id,
                                                       0);
    if (slot < 0) {
        return;
    }
    logger->frame_contexts[slot].thread_id = 0ULL;
    logger->frame_contexts[slot].frame_no = -1;
    logger->frame_contexts[slot].loop_no = -1;
    logger->frame_contexts[slot].multiframe = 0;
    logger->frame_contexts[slot].active = 0;
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
    sixel_logger_clear_frame_context_slots(logger);
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
    sixel_logger_clear_frame_context_slots(logger);
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
     * Use fully buffered output to avoid newline-triggered flushes.  FHEDT
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
    int frame_no;
    int loop_no;
    int multiframe;
    int slot;
    unsigned long long thread_id;

    /*
     * Extra arguments are accepted for compatibility with older call sites
     * but intentionally ignored to keep timeline logging lightweight.
     */
    target = sixel_logger_resolve_target(logger);
    if (target == NULL || !target->active || target->file == NULL
            || !target->mutex_ready) {
        return;
    }

    frame_no = -1;
    loop_no = -1;
    multiframe = 0;
    thread_id = sixel_logger_thread_id();

#if SIXEL_ENABLE_THREADS
    sixel_mutex_lock(&target->mutex);
#endif  /* SIXEL_ENABLE_THREADS */
    timestamp = sixel_timer_now() - target->started_at;
    if (timestamp < 0.0) {
        timestamp = 0.0;
    }
    slot = sixel_logger_find_frame_context_slot_locked(target,
                                                       thread_id,
                                                       0);
    if (slot >= 0 && target->frame_contexts[slot].active != 0) {
        frame_no = target->frame_contexts[slot].frame_no;
        loop_no = target->frame_contexts[slot].loop_no;
        multiframe = target->frame_contexts[slot].multiframe;
    }
    fprintf(target->file,
            "{\"ts\":%.6f,\"thread\":%llu,\"worker\":\"%s\","\
            "\"role\":\"%s\",\"event\":\"%s\",\"job\":%d,"\
            "\"frame_no\":%d,\"loop_no\":%d,\"multiframe\":%d}\n",
            timestamp,
            thread_id,
            worker != NULL ? worker : "",
            role != NULL ? role : "",
            event != NULL ? event : "",
            job_id,
            frame_no,
            loop_no,
            multiframe);
    fflush(target->file);
#if SIXEL_ENABLE_THREADS
    sixel_mutex_unlock(&target->mutex);
#endif  /* SIXEL_ENABLE_THREADS */
}

void
sixel_logger_set_frame_context(sixel_logger_t *logger,
                               int frame_no,
                               int loop_no,
                               int multiframe)
{
    sixel_logger_t *target;
    unsigned long long thread_id;

    target = sixel_logger_resolve_target(logger);
    if (target == NULL || !target->active || target->file == NULL
            || !target->mutex_ready) {
        return;
    }
    thread_id = sixel_logger_thread_id();
#if SIXEL_ENABLE_THREADS
    sixel_mutex_lock(&target->mutex);
#endif  /* SIXEL_ENABLE_THREADS */
    sixel_logger_set_frame_context_locked(target,
                                          thread_id,
                                          frame_no,
                                          loop_no,
                                          multiframe);
#if SIXEL_ENABLE_THREADS
    sixel_mutex_unlock(&target->mutex);
#endif  /* SIXEL_ENABLE_THREADS */
}

void
sixel_logger_clear_frame_context(sixel_logger_t *logger)
{
    sixel_logger_t *target;
    unsigned long long thread_id;

    target = sixel_logger_resolve_target(logger);
    if (target == NULL || !target->active || target->file == NULL
            || !target->mutex_ready) {
        return;
    }
    thread_id = sixel_logger_thread_id();
#if SIXEL_ENABLE_THREADS
    sixel_mutex_lock(&target->mutex);
#endif  /* SIXEL_ENABLE_THREADS */
    sixel_logger_clear_frame_context_locked(target, thread_id);
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
