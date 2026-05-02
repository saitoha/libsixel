/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
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

#include <stdio.h>

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
# if !defined(UNICODE)
#  define UNICODE
# endif
# if !defined(_UNICODE)
#  define _UNICODE
# endif
# if !defined(WIN32_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
#elif SIXEL_ENABLE_THREADS
# include <pthread.h>
#endif

#include "compat_stub.h"
#include "threading.h"
#include "timeline-logger.h"
#include "timeline-writer.h"

typedef struct sixel_timeline_writer_storage {
    sixel_timeline_writer_vtbl_t const *vtbl;
    FILE *file;
    sixel_mutex_t mutex;
    int mutex_ready;
    int env_checked;
    int active;
    unsigned int next_session_id;
} sixel_timeline_writer_storage_t;

static void
sixel_timeline_writer_flush(sixel_timeline_writer_t *writer);

static void
sixel_timeline_writer_ref(sixel_timeline_writer_t *writer)
{
    (void)writer;
}

static void
sixel_timeline_writer_unref(sixel_timeline_writer_t *writer)
{
    sixel_timeline_writer_flush(writer);
}

static SIXELSTATUS
sixel_timeline_writer_create_logger(
    sixel_timeline_writer_t *writer,
    sixel_allocator_t *allocator,
    sixel_timeline_logger_t **logger);

static SIXELSTATUS
sixel_timeline_writer_write(sixel_timeline_writer_t *writer,
                            sixel_timeline_record_t const *record);

static int
sixel_timeline_writer_enabled(sixel_timeline_writer_t const *writer);

static sixel_timeline_writer_vtbl_t const g_sixel_timeline_writer_vtbl = {
    sixel_timeline_writer_ref,
    sixel_timeline_writer_unref,
    sixel_timeline_writer_create_logger,
    sixel_timeline_writer_write,
    sixel_timeline_writer_flush,
    sixel_timeline_writer_enabled
};

static sixel_timeline_writer_storage_t g_sixel_timeline_writer = {
    &g_sixel_timeline_writer_vtbl,
    NULL,
    {0},
    0,
    0,
    0,
    1u
};

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD) && SIXEL_ENABLE_THREADS
static INIT_ONCE g_sixel_timeline_writer_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
sixel_timeline_writer_init_once(PINIT_ONCE once,
                                PVOID parameter,
                                PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;

    if (sixel_mutex_init(&g_sixel_timeline_writer.mutex) == SIXEL_OK) {
        g_sixel_timeline_writer.mutex_ready = 1;
    }
    return TRUE;
}
#elif SIXEL_ENABLE_THREADS
static pthread_once_t g_sixel_timeline_writer_once = PTHREAD_ONCE_INIT;

static void
sixel_timeline_writer_init_once(void)
{
    if (sixel_mutex_init(&g_sixel_timeline_writer.mutex) == SIXEL_OK) {
        g_sixel_timeline_writer.mutex_ready = 1;
    }
}
#endif

static SIXELSTATUS
sixel_timeline_writer_ensure_mutex(void)
{
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD) && SIXEL_ENABLE_THREADS
    BOOL initialized;

    initialized = InitOnceExecuteOnce(&g_sixel_timeline_writer_once,
                                      sixel_timeline_writer_init_once,
                                      NULL,
                                      NULL);
    if (!initialized || !g_sixel_timeline_writer.mutex_ready) {
        return SIXEL_RUNTIME_ERROR;
    }
#elif SIXEL_ENABLE_THREADS
    int once_status;

    once_status = pthread_once(&g_sixel_timeline_writer_once,
                               sixel_timeline_writer_init_once);
    if (once_status != 0 || !g_sixel_timeline_writer.mutex_ready) {
        return SIXEL_RUNTIME_ERROR;
    }
#endif
    return SIXEL_OK;
}

static void
sixel_timeline_writer_lock(sixel_timeline_writer_storage_t *writer)
{
#if SIXEL_ENABLE_THREADS
    if (writer != NULL && writer->mutex_ready) {
        sixel_mutex_lock(&writer->mutex);
    }
#else
    (void)writer;
#endif
}

static void
sixel_timeline_writer_unlock(sixel_timeline_writer_storage_t *writer)
{
#if SIXEL_ENABLE_THREADS
    if (writer != NULL && writer->mutex_ready) {
        sixel_mutex_unlock(&writer->mutex);
    }
#else
    (void)writer;
#endif
}

static void
sixel_timeline_writer_open_env_locked(
    sixel_timeline_writer_storage_t *writer)
{
    char const *path;

    if (writer == NULL || writer->env_checked != 0) {
        return;
    }
    writer->env_checked = 1;
    path = sixel_compat_getenv("SIXEL_LOG_PATH");
    if (path == NULL || path[0] == '\0') {
        return;
    }

    writer->file = sixel_compat_fopen(path, "w");
    if (writer->file == NULL) {
        writer->active = 0;
        return;
    }
    writer->active = 1;
    (void)setvbuf(writer->file, NULL, _IOFBF, BUFSIZ);
}

static SIXELSTATUS
sixel_timeline_writer_create_logger(
    sixel_timeline_writer_t *writer,
    sixel_allocator_t *allocator,
    sixel_timeline_logger_t **logger)
{
    sixel_timeline_writer_storage_t *storage;
    SIXELSTATUS status;
    unsigned int session_id;
    int enabled;

    if (writer == NULL || allocator == NULL || logger == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = (sixel_timeline_writer_storage_t *)writer;
    *logger = NULL;
    status = sixel_timeline_writer_ensure_mutex();
    if (SIXEL_FAILED(status)) {
        return status;
    }

    session_id = 0u;
    enabled = 0;
    sixel_timeline_writer_lock(storage);
    sixel_timeline_writer_open_env_locked(storage);
    session_id = storage->next_session_id;
    storage->next_session_id = storage->next_session_id + 1u;
    if (storage->next_session_id == 0u) {
        storage->next_session_id = 1u;
    }
    enabled = storage->active;
    sixel_timeline_writer_unlock(storage);

    return sixel_timeline_logger_new_with_writer(allocator,
                                                 writer,
                                                 session_id,
                                                 enabled,
                                                 logger);
}

static SIXELSTATUS
sixel_timeline_writer_write(sixel_timeline_writer_t *writer,
                            sixel_timeline_record_t const *record)
{
    sixel_timeline_writer_storage_t *storage;
    SIXELSTATUS status;
    char line[512];
    int written;

    if (writer == NULL || record == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = (sixel_timeline_writer_storage_t *)writer;
    status = sixel_timeline_writer_ensure_mutex();
    if (SIXEL_FAILED(status)) {
        return status;
    }

    written = snprintf(
        line,
        sizeof(line),
        "{\"ts\":%.6f,\"session_id\":%u,\"thread\":%llu,"
        "\"worker\":\"%s\",\"role\":\"%s\",\"event\":\"%s\","
        "\"job\":%d,\"frame_no\":%d,\"loop_no\":%d,"
        "\"multiframe\":%d}\n",
        record->timestamp,
        record->session_id,
        record->thread_id,
        record->worker != NULL ? record->worker : "",
        record->role != NULL ? record->role : "",
        record->event != NULL ? record->event : "",
        record->job_id,
        record->frame_no,
        record->loop_no,
        record->multiframe);
    if (written < 0) {
        return SIXEL_RUNTIME_ERROR;
    }
    if ((size_t)written >= sizeof(line)) {
        written = (int)sizeof(line) - 1;
        line[written] = '\0';
    }

    sixel_timeline_writer_lock(storage);
    sixel_timeline_writer_open_env_locked(storage);
    if (storage->active != 0 && storage->file != NULL) {
        if (fwrite(line, 1u, (size_t)written, storage->file) !=
                (size_t)written) {
            storage->active = 0;
            sixel_timeline_writer_unlock(storage);
            return SIXEL_RUNTIME_ERROR;
        }
    }
    sixel_timeline_writer_unlock(storage);

    return SIXEL_OK;
}

static void
sixel_timeline_writer_flush(sixel_timeline_writer_t *writer)
{
    sixel_timeline_writer_storage_t *storage;

    if (writer == NULL ||
        SIXEL_FAILED(sixel_timeline_writer_ensure_mutex())) {
        return;
    }

    storage = (sixel_timeline_writer_storage_t *)writer;
    sixel_timeline_writer_lock(storage);
    if (storage->active != 0 && storage->file != NULL) {
        (void)fflush(storage->file);
    }
    sixel_timeline_writer_unlock(storage);
}

static int
sixel_timeline_writer_enabled(sixel_timeline_writer_t const *writer)
{
    sixel_timeline_writer_storage_t *storage;
    SIXELSTATUS status;
    int enabled;

    if (writer == NULL) {
        return 0;
    }

    storage = (sixel_timeline_writer_storage_t *)writer;
    status = sixel_timeline_writer_ensure_mutex();
    if (SIXEL_FAILED(status)) {
        return 0;
    }

    sixel_timeline_writer_lock(storage);
    sixel_timeline_writer_open_env_locked(storage);
    enabled = storage->active;
    sixel_timeline_writer_unlock(storage);

    return enabled;
}

SIXELSTATUS
sixel_timeline_writer_get_default(void **writer)
{
    SIXELSTATUS status;
    sixel_timeline_writer_t *service;

    if (writer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_timeline_writer_ensure_mutex();
    if (SIXEL_FAILED(status)) {
        *writer = NULL;
        return status;
    }

    service = (sixel_timeline_writer_t *)&g_sixel_timeline_writer;
    service->vtbl->ref(service);
    *writer = service;
    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
