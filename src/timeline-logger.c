/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025-2026 libsixel developers. See `AUTHORS`.
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

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#elif SIXEL_ENABLE_THREADS
#include <pthread.h>
#endif

#include "sixel_atomic.h"
#include "threading.h"
#include "timeline-logger.h"
#include "timeline-writer.h"
#include "timer.h"

#define SIXEL_TIMELINE_LOGGER_FRAME_CONTEXT_SLOTS 64

typedef struct sixel_timeline_logger_frame_context {
    unsigned long long thread_id;
    int frame_no;
    int loop_no;
    int multiframe;
    int active;
} sixel_timeline_logger_frame_context_t;

typedef struct sixel_timeline_logger_storage {
    sixel_timeline_logger_vtbl_t const *vtbl;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_timeline_writer_t *writer;
    sixel_mutex_t mutex;
    int mutex_ready;
    int enabled;
    unsigned int session_id;
    double started_at;
    sixel_timeline_logger_frame_context_t
        frame_contexts[SIXEL_TIMELINE_LOGGER_FRAME_CONTEXT_SLOTS];
} sixel_timeline_logger_storage_t;

static unsigned long long
sixel_timeline_logger_thread_id(void)
{
#if defined(_WIN32)
    return (unsigned long long)GetCurrentThreadId();
#elif SIXEL_ENABLE_THREADS
    return (unsigned long long)(uintptr_t)pthread_self();
#else
    return 0ULL;
#endif
}

static void
sixel_timeline_logger_clear_frame_context_slots(
    sixel_timeline_logger_storage_t *logger)
{
    int index;

    if (logger == NULL) {
        return;
    }
    for (index = 0;
         index < SIXEL_TIMELINE_LOGGER_FRAME_CONTEXT_SLOTS;
         ++index) {
        logger->frame_contexts[index].thread_id = 0ULL;
        logger->frame_contexts[index].frame_no = -1;
        logger->frame_contexts[index].loop_no = -1;
        logger->frame_contexts[index].multiframe = 0;
        logger->frame_contexts[index].active = 0;
    }
}

static int
sixel_timeline_logger_find_frame_context_slot_locked(
    sixel_timeline_logger_storage_t *logger,
    unsigned long long thread_id,
    int create_if_missing)
{
    int free_index;
    int index;

    free_index = -1;
    if (logger == NULL) {
        return -1;
    }

    for (index = 0;
         index < SIXEL_TIMELINE_LOGGER_FRAME_CONTEXT_SLOTS;
         ++index) {
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

    return (int)(thread_id %
                 (unsigned long long)
                 SIXEL_TIMELINE_LOGGER_FRAME_CONTEXT_SLOTS);
}

static void
sixel_timeline_logger_lock(sixel_timeline_logger_storage_t *logger)
{
#if SIXEL_ENABLE_THREADS
    if (logger != NULL && logger->mutex_ready) {
        sixel_mutex_lock(&logger->mutex);
    }
#else
    (void)logger;
#endif
}

static void
sixel_timeline_logger_unlock(sixel_timeline_logger_storage_t *logger)
{
#if SIXEL_ENABLE_THREADS
    if (logger != NULL && logger->mutex_ready) {
        sixel_mutex_unlock(&logger->mutex);
    }
#else
    (void)logger;
#endif
}

static void
sixel_timeline_logger_ref(sixel_timeline_logger_t *logger)
{
    sixel_timeline_logger_storage_t *storage;

    if (logger == NULL) {
        return;
    }
    storage = (sixel_timeline_logger_storage_t *)logger;
    (void)sixel_atomic_fetch_add_u32(&storage->ref, 1U);
}

static void
sixel_timeline_logger_destroy(sixel_timeline_logger_storage_t *logger)
{
    sixel_allocator_t *allocator;

    if (logger == NULL) {
        return;
    }
    if (logger->writer != NULL && logger->writer->vtbl != NULL) {
        logger->writer->vtbl->flush(logger->writer);
        logger->writer->vtbl->unref(logger->writer);
        logger->writer = NULL;
    }
    if (logger->mutex_ready) {
        sixel_mutex_destroy(&logger->mutex);
        logger->mutex_ready = 0;
    }
    allocator = logger->allocator;
    if (allocator != NULL) {
        sixel_allocator_free(allocator, logger);
        sixel_allocator_unref(allocator);
    }
}

static void
sixel_timeline_logger_unref_vtbl(sixel_timeline_logger_t *logger)
{
    sixel_timeline_logger_storage_t *storage;
    unsigned int previous;

    if (logger == NULL) {
        return;
    }
    storage = (sixel_timeline_logger_storage_t *)logger;
    previous = sixel_atomic_fetch_sub_u32(&storage->ref, 1U);
    if (previous == 1U) {
        sixel_timeline_logger_destroy(storage);
    }
}

static SIXELSTATUS
sixel_timeline_logger_log(sixel_timeline_logger_t *logger,
                          sixel_timeline_event_t const *event)
{
    sixel_timeline_logger_storage_t *storage;
    sixel_timeline_record_t record;
    unsigned long long thread_id;
    int slot;

    if (logger == NULL || event == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = (sixel_timeline_logger_storage_t *)logger;
    if (storage->enabled == 0 || storage->writer == NULL ||
        storage->writer->vtbl == NULL || storage->writer->vtbl->write == NULL) {
        return SIXEL_OK;
    }

    memset(&record, 0, sizeof(record));
    record.session_id = storage->session_id;
    record.worker = event->worker;
    record.role = event->role;
    record.event = event->event;
    record.job_id = event->job_id;
    record.frame_no = -1;
    record.loop_no = -1;
    record.multiframe = 0;
    thread_id = sixel_timeline_logger_thread_id();
    record.thread_id = thread_id;

    sixel_timeline_logger_lock(storage);
    record.timestamp = sixel_timer_now() - storage->started_at;
    if (record.timestamp < 0.0) {
        record.timestamp = 0.0;
    }
    slot = sixel_timeline_logger_find_frame_context_slot_locked(storage,
                                                                thread_id,
                                                                0);
    if (slot >= 0 && storage->frame_contexts[slot].active != 0) {
        record.frame_no = storage->frame_contexts[slot].frame_no;
        record.loop_no = storage->frame_contexts[slot].loop_no;
        record.multiframe = storage->frame_contexts[slot].multiframe;
    }
    sixel_timeline_logger_unlock(storage);

    return storage->writer->vtbl->write(storage->writer, &record);
}

static SIXELSTATUS
sixel_timeline_logger_set_frame_context_vtbl(
    sixel_timeline_logger_t *logger,
    sixel_timeline_frame_context_t const *context)
{
    sixel_timeline_logger_storage_t *storage;
    unsigned long long thread_id;
    int slot;

    if (logger == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = (sixel_timeline_logger_storage_t *)logger;
    if (storage->enabled == 0) {
        return SIXEL_OK;
    }

    thread_id = sixel_timeline_logger_thread_id();
    sixel_timeline_logger_lock(storage);
    slot = sixel_timeline_logger_find_frame_context_slot_locked(storage,
                                                                thread_id,
                                                                1);
    if (slot >= 0) {
        storage->frame_contexts[slot].thread_id = thread_id;
        storage->frame_contexts[slot].frame_no = context->frame_no;
        storage->frame_contexts[slot].loop_no = context->loop_no;
        storage->frame_contexts[slot].multiframe = context->multiframe;
        storage->frame_contexts[slot].active = 1;
    }
    sixel_timeline_logger_unlock(storage);

    return SIXEL_OK;
}

static void
sixel_timeline_logger_clear_frame_context_vtbl(
    sixel_timeline_logger_t *logger)
{
    sixel_timeline_logger_storage_t *storage;
    unsigned long long thread_id;
    int slot;

    if (logger == NULL) {
        return;
    }

    storage = (sixel_timeline_logger_storage_t *)logger;
    if (storage->enabled == 0) {
        return;
    }

    thread_id = sixel_timeline_logger_thread_id();
    sixel_timeline_logger_lock(storage);
    slot = sixel_timeline_logger_find_frame_context_slot_locked(storage,
                                                                thread_id,
                                                                0);
    if (slot >= 0) {
        storage->frame_contexts[slot].thread_id = 0ULL;
        storage->frame_contexts[slot].frame_no = -1;
        storage->frame_contexts[slot].loop_no = -1;
        storage->frame_contexts[slot].multiframe = 0;
        storage->frame_contexts[slot].active = 0;
    }
    sixel_timeline_logger_unlock(storage);
}

static void
sixel_timeline_logger_flush_vtbl(sixel_timeline_logger_t *logger)
{
    sixel_timeline_logger_storage_t *storage;

    if (logger == NULL) {
        return;
    }
    storage = (sixel_timeline_logger_storage_t *)logger;
    if (storage->writer != NULL && storage->writer->vtbl != NULL &&
        storage->writer->vtbl->flush != NULL) {
        storage->writer->vtbl->flush(storage->writer);
    }
}

static int
sixel_timeline_logger_enabled_vtbl(
    sixel_timeline_logger_t const *logger)
{
    sixel_timeline_logger_storage_t const *storage;

    if (logger == NULL) {
        return 0;
    }
    storage = (sixel_timeline_logger_storage_t const *)logger;
    return storage->enabled;
}

static unsigned int
sixel_timeline_logger_session_id_vtbl(
    sixel_timeline_logger_t const *logger)
{
    sixel_timeline_logger_storage_t const *storage;

    if (logger == NULL) {
        return 0u;
    }
    storage = (sixel_timeline_logger_storage_t const *)logger;
    return storage->session_id;
}

static sixel_timeline_logger_vtbl_t const g_sixel_timeline_logger_vtbl = {
    sixel_timeline_logger_ref,
    sixel_timeline_logger_unref_vtbl,
    sixel_timeline_logger_log,
    sixel_timeline_logger_set_frame_context_vtbl,
    sixel_timeline_logger_clear_frame_context_vtbl,
    sixel_timeline_logger_flush_vtbl,
    sixel_timeline_logger_enabled_vtbl,
    sixel_timeline_logger_session_id_vtbl
};

SIXELSTATUS
sixel_timeline_logger_new_with_writer(
    sixel_allocator_t *allocator,
    sixel_timeline_writer_t *writer,
    unsigned int session_id,
    int enabled,
    sixel_timeline_logger_t **logger)
{
    sixel_timeline_logger_storage_t *storage;

    if (allocator == NULL || writer == NULL || logger == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *logger = NULL;
    storage = (sixel_timeline_logger_storage_t *)sixel_allocator_malloc(
        allocator,
        sizeof(*storage));
    if (storage == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memset(storage, 0, sizeof(*storage));
    storage->vtbl = &g_sixel_timeline_logger_vtbl;
    storage->ref = 1U;
    storage->allocator = allocator;
    sixel_allocator_ref(allocator);
    storage->writer = writer;
    if (writer->vtbl != NULL && writer->vtbl->ref != NULL) {
        writer->vtbl->ref(writer);
    }
    storage->enabled = enabled != 0 ? 1 : 0;
    storage->session_id = session_id;
    storage->started_at = sixel_timer_now();
    sixel_timeline_logger_clear_frame_context_slots(storage);
#if SIXEL_ENABLE_THREADS
    if (sixel_mutex_init(&storage->mutex) != SIXEL_OK) {
        sixel_timeline_logger_destroy(storage);
        return SIXEL_RUNTIME_ERROR;
    }
    storage->mutex_ready = 1;
#endif

    *logger = (sixel_timeline_logger_t *)storage;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_timeline_logger_factory_new(sixel_allocator_t *allocator, void **object)
{
    SIXELSTATUS status;
    sixel_timeline_writer_t *writer;
    void *service;

    if (allocator == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *object = NULL;
    writer = NULL;
    service = NULL;
    status = sixel_timeline_writer_get_default(&service);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    writer = (sixel_timeline_writer_t *)service;
    if (writer == NULL || writer->vtbl == NULL ||
        writer->vtbl->create_logger == NULL) {
        if (writer != NULL && writer->vtbl != NULL &&
            writer->vtbl->unref != NULL) {
            writer->vtbl->unref(writer);
        }
        return SIXEL_BAD_ARGUMENT;
    }

    status = writer->vtbl->create_logger(writer,
                                         allocator,
                                         (sixel_timeline_logger_t **)object);
    writer->vtbl->unref(writer);
    return status;
}

SIXELSTATUS
sixel_timeline_logger_prepare_env(
    sixel_allocator_t *allocator,
    sixel_timeline_logger_t **logger)
{
    SIXELSTATUS status;
    sixel_allocator_t *local_allocator;
    sixel_timeline_writer_t *writer;
    void *service;
    int allocator_owned;

    if (logger == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *logger = NULL;
    local_allocator = allocator;
    writer = NULL;
    service = NULL;
    allocator_owned = 0;
    if (local_allocator == NULL) {
        status = sixel_allocator_new(&local_allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        allocator_owned = 1;
    }

    status = sixel_timeline_writer_get_default(&service);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    writer = (sixel_timeline_writer_t *)service;
    if (writer == NULL || writer->vtbl == NULL ||
        writer->vtbl->enabled == NULL ||
        writer->vtbl->create_logger == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (!writer->vtbl->enabled(writer)) {
        status = SIXEL_OK;
        goto end;
    }

    status = writer->vtbl->create_logger(writer, local_allocator, logger);

end:
    if (writer != NULL && writer->vtbl != NULL &&
        writer->vtbl->unref != NULL) {
        writer->vtbl->unref(writer);
    }
    if (allocator_owned && local_allocator != NULL) {
        sixel_allocator_unref(local_allocator);
    }
    return status;
}

void
sixel_timeline_logger_unref(sixel_timeline_logger_t *logger)
{
    if (logger != NULL && logger->vtbl != NULL &&
        logger->vtbl->unref != NULL) {
        logger->vtbl->unref(logger);
    }
}

int
sixel_timeline_logger_is_enabled(sixel_timeline_logger_t const *logger)
{
    if (logger == NULL || logger->vtbl == NULL ||
        logger->vtbl->enabled == NULL) {
        return 0;
    }
    return logger->vtbl->enabled(logger);
}

unsigned int
sixel_timeline_logger_session_id(sixel_timeline_logger_t const *logger)
{
    if (logger == NULL || logger->vtbl == NULL ||
        logger->vtbl->session_id == NULL) {
        return 0u;
    }
    return logger->vtbl->session_id(logger);
}

void
sixel_timeline_logger_logf(sixel_timeline_logger_t *logger,
                           char const *role,
                           char const *worker,
                           char const *event,
                           int job_id,
                           ...)
{
    sixel_timeline_event_t record;

    if (logger == NULL || logger->vtbl == NULL ||
        logger->vtbl->log == NULL) {
        return;
    }

    /*
     * Older call sites pass diagnostic details after job_id.  Timeline charts
     * currently consume the structural fields only, so keep the wrapper
     * variadic for compatibility and intentionally ignore the extra payload.
     */
    record.role = role;
    record.worker = worker;
    record.event = event;
    record.job_id = job_id;
    (void)logger->vtbl->log(logger, &record);
}

void
sixel_timeline_logger_set_frame_context(sixel_timeline_logger_t *logger,
                                        int frame_no,
                                        int loop_no,
                                        int multiframe)
{
    sixel_timeline_frame_context_t context;

    if (logger == NULL || logger->vtbl == NULL ||
        logger->vtbl->set_frame_context == NULL) {
        return;
    }

    context.frame_no = frame_no;
    context.loop_no = loop_no;
    context.multiframe = multiframe;
    (void)logger->vtbl->set_frame_context(logger, &context);
}

void
sixel_timeline_logger_clear_frame_context(sixel_timeline_logger_t *logger)
{
    if (logger == NULL || logger->vtbl == NULL ||
        logger->vtbl->clear_frame_context == NULL) {
        return;
    }
    logger->vtbl->clear_frame_context(logger);
}

void
sixel_timeline_logger_flush(sixel_timeline_logger_t *logger)
{
    if (logger == NULL || logger->vtbl == NULL ||
        logger->vtbl->flush == NULL) {
        return;
    }
    logger->vtbl->flush(logger);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
