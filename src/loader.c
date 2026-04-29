/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
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
#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDARG_H
# include <stdarg.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#elif HAVE_TIME_H
# include <time.h>
#endif  /* HAVE_SYS_TIME_H HAVE_TIME_H */
#if defined(_WIN32)
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
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#if HAVE_SPAWN_H
# include <spawn.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_DIRENT_H
# include <dirent.h>
#endif

#include <sixel.h>
#include "loader.h"
#include "loader-builtin.h"
#include "loader-common.h"
#include "loader-coregraphics.h"
#include "loader-gd.h"
#include "loader-gdk-pixbuf2.h"
#include "loader-libjpeg.h"
#include "loader-libpng.h"
#include "loader-librsvg.h"
#include "loader-order-schema.h"
#include "loader-manager.h"
#include "loader-wic.h"
#include "factory.h"
#include "compat_stub.h"
#include "frame.h"
#include "chunk.h"
#include "allocator.h"
#include "encoder.h"
#include "logger.h"
#include "options.h"
#include "tty.h"
#include "threading.h"
#include "sleep.h"
#include "sixel_atomic.h"

#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

#define SIXEL_LOADER_OSC11_BG_QUERY_ENV "SIXEL_LOADER_OSC11_BG_QUERY"
#define SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_ENV \
    "SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_MS"
#define SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_DEFAULT_MS 50

/*
 * Internal loader state carried across backends.  The fields mirror the
 * original `loader.c` layout to keep statistics, logging, and allocator
 * ownership centralized while implementations move into per-backend files.
 */
struct sixel_loader {
    sixel_atomic_u32_t ref;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    unsigned char bgcolor[3];
    int has_bgcolor;
    int bgcolor_source;
    int loop_control;
    int finsecure;
    int has_start_frame_no;
    int start_frame_no;
    int const *cancel_flag;
    sixel_cancel_function cancel_function;
    void *cancel_context;
    void *context;
    sixel_logger_t logger;
    char *loader_order;
    sixel_option_argument_list_resolution_t loader_order_resolution;
    sixel_allocator_t *allocator;
    int callback_failed;
    char log_path[PATH_MAX];
    int log_timeline_job_seq;
    int timeline_manager_select_job;
    int timeline_manager_select_open;
    int timeline_candidate_select_job;
    int timeline_candidate_select_open;
    char timeline_candidate_worker[96];
};

typedef struct sixel_loader_callback_state {
    sixel_loader_t *loader;
    sixel_load_image_function fn;
    void *context;
} sixel_loader_callback_state_t;

typedef struct sixel_loader_osc11_bg_query_job {
    sixel_thread_t thread;
    sixel_atomic_u32_t finished;
    sixel_atomic_u32_t stop_requested;
    unsigned char bgcolor[3];
    SIXELSTATUS status;
    int timeout_ms;
    int started;
} sixel_loader_osc11_bg_query_job_t;

static int
loader_osc11_bg_query_thread_main(void *context);

static int
loader_osc11_bg_query_should_stop(void *context);

static void
loader_osc11_bg_query_job_init(sixel_loader_osc11_bg_query_job_t *job);

static int
loader_osc11_bg_query_job_is_finished(void *context);

static int
loader_osc11_bg_query_job_apply_if_ready(
    sixel_loader_t *loader,
    sixel_loader_osc11_bg_query_job_t *job);

static void
loader_osc11_bg_query_job_join(sixel_loader_osc11_bg_query_job_t *job);

static int
loader_can_query_osc11_bgcolor(sixel_loader_t const *loader);

SIXEL_INTERNAL_API void
sixel_loader_component_ref(sixel_loader_component_t *component)
{
    if (component == NULL || component->vtbl == NULL ||
        component->vtbl->ref == NULL) {
        return;
    }
    component->vtbl->ref(component);
}

SIXEL_INTERNAL_API void
sixel_loader_component_unref(sixel_loader_component_t *component)
{
    if (component == NULL || component->vtbl == NULL ||
        component->vtbl->unref == NULL) {
        return;
    }
    component->vtbl->unref(component);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_loader_component_setopt(sixel_loader_component_t *component,
                              int option,
                              void const *value)
{
    if (component == NULL || component->vtbl == NULL ||
        component->vtbl->setopt == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    return component->vtbl->setopt(component, option, value);
}

SIXEL_INTERNAL_API int
sixel_loader_component_predicate(sixel_loader_component_t *component,
                                 sixel_chunk_t const *chunk)
{
    if (component == NULL || component->vtbl == NULL ||
        component->vtbl->predicate == NULL) {
        return 1;
    }
    return component->vtbl->predicate(component, chunk);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_loader_component_load(sixel_loader_component_t *component,
                            sixel_chunk_t const *chunk,
                            sixel_load_image_function fn_load,
                            void *context)
{
    if (component == NULL || component->vtbl == NULL ||
        component->vtbl->load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    return component->vtbl->load(component, chunk, fn_load, context);
}

SIXEL_INTERNAL_API char const *
sixel_loader_component_get_name(sixel_loader_component_t const *component)
{
    if (component == NULL || component->vtbl == NULL ||
        component->vtbl->name == NULL) {
        return NULL;
    }
    return component->vtbl->name(component);
}

SIXEL_INTERNAL_API int
sixel_loader_callback_is_canceled(void *data)
{
    sixel_loader_callback_state_t *state;
    sixel_loader_t *loader;
    void *actual_context;
    int canceled;

    actual_context = loader_timeline_unwrap_callback_context(data);
    state = (sixel_loader_callback_state_t *)actual_context;
    loader = NULL;
    canceled = 0;
    if (state == NULL || state->loader == NULL) {
        return 0;
    }
    loader = state->loader;
    if (loader->cancel_function != NULL) {
        canceled = loader->cancel_function(loader->cancel_context);
    }
    if (canceled != 0) {
        return 1;
    }
    if (loader->cancel_flag == NULL) {
        return 0;
    }

    return *loader->cancel_flag != 0;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_loader_set_cancel_callback(
    sixel_loader_t *loader,
    sixel_cancel_function cancel_function,
    void *cancel_context)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;
    if (loader == NULL) {
        sixel_helper_set_additional_message(
            "sixel_loader_set_cancel_callback: loader is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    loader->cancel_function = cancel_function;
    loader->cancel_context = cancel_context;
    status = SIXEL_OK;

end:
    return status;
}


static char *
loader_strdup(char const *text, sixel_allocator_t *allocator)
{
    char *copy;
    size_t length;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text) + 1;
    copy = (char *)sixel_allocator_malloc(allocator, length);
    if (copy == NULL) {
        return NULL;
    }

    /* Copy the terminating NUL byte as part of length. */
    memcpy(copy, text, length);

    return copy;
}

int
sixel_loader_is_osc11_bg_query_enabled(char const *value)
{
    if (value == NULL) {
        return 0;
    }

    if (strcmp(value, "1") == 0) {
        return 1;
    }

    return 0;
}

int
sixel_loader_parse_osc11_bg_query_timeout_ms(char const *value)
{
    long parsed;
    char *endptr;

    parsed = 0L;
    endptr = NULL;

    if (value == NULL || value[0] == '\0') {
        return SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_DEFAULT_MS;
    }

    errno = 0;
    parsed = strtol(value, &endptr, 10);
    if (endptr == value || *endptr != '\0' ||
            errno == ERANGE || parsed < 0L ||
            parsed > (long)INT_MAX) {
        return SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_DEFAULT_MS;
    }

    return (int)parsed;
}

int
sixel_loader_wait_for_condition(sixel_loader_wait_predicate_t predicate,
                                void *context,
                                int timeout_ms)
{
    int elapsed;

    elapsed = 0;

    if (predicate == NULL) {
        return 0;
    }
    if (timeout_ms < 0) {
        timeout_ms = 0;
    }

    if (predicate(context)) {
        return 1;
    }

    /*
     * Keep polling intervals short so the loader can continue as soon as the
     * background query completes, while still bounding total wait time.
     */
    while (elapsed < timeout_ms) {
        sixel_sleep(1000u);
        if (predicate(context)) {
            return 1;
        }
        ++elapsed;
    }

    return predicate(context);
}

static int
loader_osc11_bg_query_thread_main(void *context)
{
    sixel_loader_osc11_bg_query_job_t *job;

    job = (sixel_loader_osc11_bg_query_job_t *)context;
    if (job == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    job->status = sixel_tty_query_osc11_bgcolor_with_drain(
        job->bgcolor,
        job->timeout_ms,
        loader_osc11_bg_query_should_stop,
        job);
    sixel_fence_release();
    (void)sixel_atomic_fetch_add_u32(&job->finished, 1u);

    return job->status;
}

static int
loader_osc11_bg_query_should_stop(void *context)
{
    sixel_loader_osc11_bg_query_job_t *job;
    unsigned int stop_requested;

    job = (sixel_loader_osc11_bg_query_job_t *)context;
    stop_requested = 0u;
    if (job == NULL) {
        return 1;
    }

    stop_requested = sixel_atomic_fetch_add_u32(&job->stop_requested, 0u);
    if (stop_requested != 0u) {
        return 1;
    }

    return 0;
}

static void
loader_osc11_bg_query_job_init(sixel_loader_osc11_bg_query_job_t *job)
{
    if (job == NULL) {
        return;
    }

    job->finished = 0u;
    job->stop_requested = 0u;
    job->bgcolor[0] = 0u;
    job->bgcolor[1] = 0u;
    job->bgcolor[2] = 0u;
    job->status = SIXEL_FALSE;
    job->timeout_ms = SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_DEFAULT_MS;
    job->started = 0;
}

static int
loader_osc11_bg_query_job_is_finished(void *context)
{
    sixel_loader_osc11_bg_query_job_t *job;
    unsigned int finished;

    job = (sixel_loader_osc11_bg_query_job_t *)context;
    finished = 0u;

    if (job == NULL || job->started == 0) {
        return 0;
    }

    finished = sixel_atomic_fetch_add_u32(&job->finished, 0u);
    if (finished == 0u) {
        return 0;
    }

    sixel_fence_acquire();
    return 1;
}

static int
loader_osc11_bg_query_job_apply_if_ready(
    sixel_loader_t *loader,
    sixel_loader_osc11_bg_query_job_t *job)
{
    if (loader == NULL || job == NULL) {
        return 0;
    }
    if (loader_osc11_bg_query_job_is_finished(job) == 0) {
        return 0;
    }
    if (SIXEL_FAILED(job->status)) {
        return 0;
    }

    loader->bgcolor[0] = job->bgcolor[0];
    loader->bgcolor[1] = job->bgcolor[1];
    loader->bgcolor[2] = job->bgcolor[2];
    loader->has_bgcolor = 1;

    return 1;
}

static void
loader_osc11_bg_query_job_join(sixel_loader_osc11_bg_query_job_t *job)
{
    if (job == NULL || job->started == 0) {
        return;
    }

    /*
     * Ask the reader thread to stop only when loader teardown begins. This
     * keeps draining late OSC11 bytes during the active conversion window.
     */
    (void)sixel_atomic_fetch_add_u32(&job->stop_requested, 1u);
    sixel_thread_join(&job->thread);
    job->started = 0;
}

static int
loader_can_query_osc11_bgcolor(sixel_loader_t const *loader)
{
    char const *env_value;

    env_value = sixel_compat_getenv(SIXEL_LOADER_OSC11_BG_QUERY_ENV);
    if (loader == NULL || loader->has_bgcolor != 0) {
        return 0;
    }
    if (!sixel_loader_is_osc11_bg_query_enabled(env_value)) {
        return 0;
    }
    if (sixel_compat_isatty(STDOUT_FILENO) == 0 &&
            sixel_compat_isatty(STDERR_FILENO) == 0) {
        return 0;
    }

    return 1;
}



static int
loader_timeline_next_job(sixel_loader_t *loader)
{
    int job_id;

    job_id = -1;
    if (loader == NULL) {
        return -1;
    }
    if (loader->log_timeline_job_seq < 0) {
        loader->log_timeline_job_seq = 0;
    }
    job_id = loader->log_timeline_job_seq;
    loader->log_timeline_job_seq = job_id + 1;
    return job_id;
}

static void
loader_log_timeline_event(sixel_loader_t *loader,
                          char const *worker,
                          char const *role,
                          char const *event,
                          int job_id)
{
    sixel_logger_t *logger;

    logger = NULL;
    if (loader != NULL) {
        logger = &loader->logger;
    }
    if (logger == NULL || !logger->active ||
            worker == NULL || role == NULL || event == NULL || job_id < 0) {
        return;
    }

    sixel_logger_logf(logger,
                      role,
                      worker,
                      event,
                      job_id,
                      -1,
                      0,
                      0,
                      0,
                      0,
                      "");
}

static void
loader_timeline_select_phase_start(
    sixel_loader_t *loader,
    char const *worker,
    int *job_id_out,
    int *open_out)
{
    int job_id;

    job_id = -1;
    if (job_id_out != NULL) {
        *job_id_out = -1;
    }
    if (open_out != NULL) {
        *open_out = 0;
    }
    if (loader == NULL || worker == NULL || worker[0] == '\0') {
        return;
    }

    job_id = loader_timeline_next_job(loader);
    if (job_id < 0) {
        return;
    }

    loader_log_timeline_event(loader,
                              worker,
                              "loader/select",
                              "start",
                              job_id);
    if (job_id_out != NULL) {
        *job_id_out = job_id;
    }
    if (open_out != NULL) {
        *open_out = 1;
    }
}

static void
loader_timeline_select_phase_finish(
    sixel_loader_t *loader,
    char const *worker,
    int *job_id_io,
    int *open_io,
    char const *event)
{
    int job_id;

    job_id = -1;
    if (loader == NULL || worker == NULL || worker[0] == '\0' ||
            job_id_io == NULL || open_io == NULL ||
            event == NULL || *open_io == 0) {
        return;
    }

    job_id = *job_id_io;
    if (job_id >= 0) {
        loader_log_timeline_event(loader,
                                  worker,
                                  "loader/select",
                                  event,
                                  job_id);
    }
    *job_id_io = -1;
    *open_io = 0;
}

static void
loader_timeline_select_suspend_for_callback(
    sixel_loader_t *loader,
    int *resume_manager,
    int *resume_candidate)
{
    if (resume_manager != NULL) {
        *resume_manager = 0;
    }
    if (resume_candidate != NULL) {
        *resume_candidate = 0;
    }
    if (loader == NULL) {
        return;
    }

    if (loader->timeline_candidate_select_open != 0 &&
            loader->timeline_candidate_worker[0] != '\0') {
        loader_timeline_select_phase_finish(
            loader,
            loader->timeline_candidate_worker,
            &loader->timeline_candidate_select_job,
            &loader->timeline_candidate_select_open,
            "finish");
        if (resume_candidate != NULL) {
            *resume_candidate = 1;
        }
    }
    if (loader->timeline_manager_select_open != 0) {
        loader_timeline_select_phase_finish(
            loader,
            "loader/manager",
            &loader->timeline_manager_select_job,
            &loader->timeline_manager_select_open,
            "finish");
        if (resume_manager != NULL) {
            *resume_manager = 1;
        }
    }
}

static void
loader_timeline_select_emit_immediate_failure(
    sixel_loader_t *loader,
    char const *worker)
{
    int job_id;
    int open_flag;

    job_id = -1;
    open_flag = 0;
    if (loader == NULL || worker == NULL || worker[0] == '\0') {
        return;
    }

    loader_timeline_select_phase_start(loader,
                                       worker,
                                       &job_id,
                                       &open_flag);
    loader_timeline_select_phase_finish(loader,
                                        worker,
                                        &job_id,
                                        &open_flag,
                                        "fail");
}

static void
loader_timeline_select_resume_after_callback(
    sixel_loader_t *loader,
    int resume_manager,
    int resume_candidate,
    SIXELSTATUS callback_status)
{
    if (loader == NULL) {
        return;
    }

    if (SIXEL_FAILED(callback_status)) {
        if (resume_candidate != 0 &&
                loader->timeline_candidate_worker[0] != '\0') {
            loader_timeline_select_emit_immediate_failure(
                loader,
                loader->timeline_candidate_worker);
        }
        if (resume_manager != 0) {
            loader_timeline_select_emit_immediate_failure(
                loader,
                "loader/manager");
        }
        return;
    }

    if (resume_manager != 0) {
        loader_timeline_select_phase_start(
            loader,
            "loader/manager",
            &loader->timeline_manager_select_job,
            &loader->timeline_manager_select_open);
    }
    if (resume_candidate != 0 &&
            loader->timeline_candidate_worker[0] != '\0') {
        loader_timeline_select_phase_start(
            loader,
            loader->timeline_candidate_worker,
            &loader->timeline_candidate_select_job,
            &loader->timeline_candidate_select_open);
    }
}

static SIXELSTATUS
loader_callback_trampoline(sixel_frame_t *frame, void *data)
{
    sixel_loader_callback_state_t *state;
    SIXELSTATUS status;
    int resume_manager_select;
    int resume_candidate_select;

    state = (sixel_loader_callback_state_t *)data;
    resume_manager_select = 0;
    resume_candidate_select = 0;
    if (state == NULL || state->fn == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (state->loader != NULL) {
        /*
         * loader/select represents loader-side work only. Suspend these spans
         * while control is in the downstream frame callback.
         */
        loader_timeline_select_suspend_for_callback(state->loader,
                                                    &resume_manager_select,
                                                    &resume_candidate_select);
    }
    status = state->fn(frame, state->context);
    if (state->loader != NULL) {
        loader_timeline_select_resume_after_callback(state->loader,
                                                     resume_manager_select,
                                                     resume_candidate_select,
                                                     status);
    }
    if (SIXEL_FAILED(status) && state->loader != NULL) {
        state->loader->callback_failed = 1;
    }

    return status;
}

SIXEL_INTERNAL_API void
sixel_loader_timeline_candidate_select_start(sixel_loader_t *loader,
                                             char const *worker)
{
    if (loader == NULL || worker == NULL || worker[0] == '\0') {
        return;
    }

    (void)sixel_compat_snprintf(loader->timeline_candidate_worker,
                                sizeof(loader->timeline_candidate_worker),
                                "%s",
                                worker);
    loader_timeline_select_phase_start(
        loader,
        loader->timeline_candidate_worker,
        &loader->timeline_candidate_select_job,
        &loader->timeline_candidate_select_open);
}

SIXEL_INTERNAL_API void
sixel_loader_timeline_candidate_select_finish(sixel_loader_t *loader,
                                              SIXELSTATUS status)
{
    char const *event;

    event = NULL;
    if (loader == NULL || loader->timeline_candidate_worker[0] == '\0') {
        return;
    }

    event = SIXEL_SUCCEEDED(status) ? "finish" : "fail";
    loader_timeline_select_phase_finish(
        loader,
        loader->timeline_candidate_worker,
        &loader->timeline_candidate_select_job,
        &loader->timeline_candidate_select_open,
        event);
    loader->timeline_candidate_worker[0] = '\0';
}


SIXELAPI SIXELSTATUS
sixel_loader_new(
    sixel_loader_t   /* out */ **pploader,
    sixel_allocator_t/* in */  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_loader_t *loader;
    sixel_allocator_t *local_allocator;

    loader = NULL;
    local_allocator = allocator;

    if (pploader == NULL) {
        sixel_helper_set_additional_message(
            "sixel_loader_new: pploader is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (local_allocator == NULL) {
        status = sixel_allocator_new(&local_allocator,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        sixel_allocator_ref(local_allocator);
    }

    loader = (sixel_loader_t *)sixel_allocator_malloc(local_allocator,
                                                      sizeof(*loader));
    if (loader == NULL) {
        sixel_helper_set_additional_message(
            "sixel_loader_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        sixel_allocator_unref(local_allocator);
        goto end;
    }

    loader->ref = 1U;
    loader->fstatic = 0;
    /*
     * Default policy: keep source palettes when a loader can provide them.
     * This avoids unnecessary requantization loss in downstream encoding.
     */
    loader->fuse_palette = 1;
    loader->reqcolors = SIXEL_PALETTE_MAX;
    loader->bgcolor[0] = 0;
    loader->bgcolor[1] = 0;
    loader->bgcolor[2] = 0;
    loader->has_bgcolor = 0;
    loader->bgcolor_source = SIXEL_LOADER_BGCOLOR_SOURCE_EXPLICIT;
    loader->loop_control = SIXEL_LOOP_AUTO;
    loader->finsecure = 0;
    loader->has_start_frame_no = 0;
    loader->start_frame_no = INT_MIN;
    loader->cancel_flag = NULL;
    loader->cancel_function = NULL;
    loader->cancel_context = NULL;
    loader->context = NULL;
    /*
     * Initialize a private logger. The helper reuses an existing global
     * logger sink when present so loader markers share the timeline with
     * upstream stages without requiring sixel_loader_setopt().
     */
    sixel_logger_init(&loader->logger);
    (void)sixel_logger_prepare_env(&loader->logger);
    loader->loader_order = NULL;
    sixel_option_init_argument_list_resolution(
        &loader->loader_order_resolution);
    loader->allocator = local_allocator;
    loader->callback_failed = 0;
    loader->log_path[0] = '\0';
    loader->log_timeline_job_seq = 0;
    loader->timeline_manager_select_job = -1;
    loader->timeline_manager_select_open = 0;
    loader->timeline_candidate_select_job = -1;
    loader->timeline_candidate_select_open = 0;
    loader->timeline_candidate_worker[0] = '\0';

    *pploader = loader;
    status = SIXEL_OK;

end:
    return status;
}

SIXELAPI void
sixel_loader_ref(
    sixel_loader_t /* in */ *loader)
{
    if (loader == NULL) {
        return;
    }

    (void)sixel_atomic_fetch_add_u32(&loader->ref, 1U);
}

SIXELAPI void
sixel_loader_unref(
    sixel_loader_t /* in */ *loader)
{
    sixel_allocator_t *allocator;
    unsigned int previous;

    if (loader == NULL) {
        return;
    }

    previous = sixel_atomic_fetch_sub_u32(&loader->ref, 1U);
    if (previous == 1U) {
        allocator = loader->allocator;
        sixel_logger_close(&loader->logger);
        sixel_allocator_free(allocator, loader->loader_order);
        sixel_option_free_argument_list_resolution(
            &loader->loader_order_resolution);
        sixel_allocator_free(allocator, loader);
        sixel_allocator_unref(allocator);
    }
}

SIXELAPI SIXELSTATUS
sixel_loader_setopt(
    sixel_loader_t /* in */ *loader,
    int            /* in */ option,
    void const     /* in */ *value)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int const *flag;
    unsigned char const *color;
    char const *order;
    char const *canonical_order;
    char *copy;
    sixel_allocator_t *allocator;
    char match_detail[128];
    sixel_option_argument_list_resolution_t parsed_order;

    flag = NULL;
    color = NULL;
    order = NULL;
    canonical_order = NULL;
    copy = NULL;
    allocator = NULL;
    match_detail[0] = '\0';
    sixel_option_init_argument_list_resolution(&parsed_order);

    if (loader == NULL) {
        sixel_helper_set_additional_message(
            "sixel_loader_setopt: loader is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end0;
    }

    sixel_loader_ref(loader);

    switch (option) {
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        flag = (int const *)value;
        loader->fstatic = flag != NULL ? *flag : 0;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        flag = (int const *)value;
        loader->fuse_palette = flag != NULL ? *flag : 1;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        flag = (int const *)value;
        loader->reqcolors = flag != NULL ? *flag : SIXEL_PALETTE_MAX;
        if (loader->reqcolors < 1) {
            sixel_helper_set_additional_message(
                "sixel_loader_setopt: reqcolors must be 1 or greater.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (loader->reqcolors > SIXEL_PALETTE_MAX) {
            loader->reqcolors = SIXEL_PALETTE_MAX;
        }
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_BGCOLOR:
        if (value == NULL) {
            loader->has_bgcolor = 0;
        } else {
            color = (unsigned char const *)value;
            loader->bgcolor[0] = color[0];
            loader->bgcolor[1] = color[1];
            loader->bgcolor[2] = color[2];
            loader->has_bgcolor = 1;
            if (loader->bgcolor_source != SIXEL_LOADER_BGCOLOR_SOURCE_ENV) {
                loader->bgcolor_source = SIXEL_LOADER_BGCOLOR_SOURCE_EXPLICIT;
            }
        }
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_BGCOLOR_SOURCE:
        flag = (int const *)value;
        if (flag == NULL) {
            loader->bgcolor_source = SIXEL_LOADER_BGCOLOR_SOURCE_EXPLICIT;
            status = SIXEL_OK;
            break;
        }
        if (*flag != SIXEL_LOADER_BGCOLOR_SOURCE_EXPLICIT &&
            *flag != SIXEL_LOADER_BGCOLOR_SOURCE_ENV) {
            sixel_helper_set_additional_message(
                "sixel_loader_setopt: bgcolor source is invalid.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        loader->bgcolor_source = *flag;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_LOOP_CONTROL:
        flag = (int const *)value;
        loader->loop_control = flag != NULL ? *flag : SIXEL_LOOP_AUTO;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_INSECURE:
        flag = (int const *)value;
        loader->finsecure = flag != NULL ? *flag : 0;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        if (value == NULL) {
            loader->has_start_frame_no = 0;
            loader->start_frame_no = INT_MIN;
        } else {
            flag = (int const *)value;
            loader->start_frame_no = *flag;
            loader->has_start_frame_no = 1;
        }
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_CANCEL_FLAG:
        loader->cancel_flag = (int const *)value;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_LOADER_ORDER:
        allocator = loader->allocator;
        if (value != NULL) {
            order = (char const *)value;
            status = sixel_loader_order_parse_and_validate(order,
                                                           &parsed_order,
                                                           match_detail,
                                                           sizeof(
                                                               match_detail));
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            canonical_order = parsed_order.canonical_argument;
            if (canonical_order == NULL) {
                canonical_order = "";
            }
            copy = loader_strdup(canonical_order, allocator);
            if (copy == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_loader_setopt: loader_strdup() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }
        sixel_allocator_free(allocator, loader->loader_order);
        loader->loader_order = copy;
        copy = NULL;
        if (value != NULL) {
            sixel_option_move_argument_list_resolution(
                &loader->loader_order_resolution,
                &parsed_order);
        } else {
            sixel_option_free_argument_list_resolution(
                &loader->loader_order_resolution);
        }
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_CONTEXT:
        loader->context = (void *)value;
        status = SIXEL_OK;
        break;
    default:
        sixel_helper_set_additional_message(
            "sixel_loader_setopt: unknown option.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

end:
    if (copy != NULL) {
        sixel_allocator_free(loader->allocator, copy);
    }
    sixel_option_free_argument_list_resolution(&parsed_order);
    sixel_loader_unref(loader);

end0:
    return status;
}

SIXEL_INTERNAL_API int
sixel_loader_get_start_frame_no(sixel_loader_t const *loader,
                                int *start_frame_no)
{
    if (start_frame_no != NULL) {
        *start_frame_no = INT_MIN;
    }
    if (loader == NULL || start_frame_no == NULL ||
        loader->has_start_frame_no == 0) {
        return 0;
    }

    *start_frame_no = loader->start_frame_no;
    return 1;
}

SIXELAPI SIXELSTATUS
sixel_loader_load_file(
    sixel_loader_t         /* in */ *loader,
    char const             /* in */ *filename,
    sixel_load_image_function /* in */ fn_load)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_chunk_t *pchunk;
    sixel_factory_t *factory;
    sixel_loader_manager_t *manager;
    int reqcolors;
    char const *order_override;
    char const *env_order;
    sixel_option_argument_list_resolution_t const *active_order_resolution;
    sixel_loader_callback_state_t callback_state;
    sixel_option_argument_list_resolution_t order_resolution;
    sixel_loader_suboptions_t active_suboptions;
    sixel_loader_manager_build_request_t build_request;
    sixel_loader_osc11_bg_query_job_t osc11_query_job;
    char const *osc11_timeout_env;
    int osc11_timeout_ms;
    int osc11_bgcolor_applied;
    int thread_status;
    int wait_result;
    int chunk_job_id;

    pchunk = NULL;
    factory = NULL;
    manager = NULL;
    reqcolors = 0;
    order_override = NULL;
    env_order = NULL;
    active_order_resolution = NULL;
    osc11_timeout_env = NULL;
    osc11_timeout_ms = SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_DEFAULT_MS;
    osc11_bgcolor_applied = 0;
    thread_status = SIXEL_FALSE;
    wait_result = 0;
    chunk_job_id = -1;
    sixel_option_init_argument_list_resolution(&order_resolution);
    loader_manager_init_loader_suboptions(&active_suboptions);
    loader_osc11_bg_query_job_init(&osc11_query_job);
    memset(&build_request, 0, sizeof(build_request));

    if (loader == NULL) {
        sixel_helper_set_additional_message(
            "sixel_loader_load_file: loader is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end0;
    }

    sixel_loader_ref(loader);

    loader->log_timeline_job_seq = 0;
    loader->timeline_manager_select_job = -1;
    loader->timeline_manager_select_open = 0;
    loader->timeline_candidate_select_job = -1;
    loader->timeline_candidate_select_open = 0;
    loader->timeline_candidate_worker[0] = '\0';
    loader->log_path[0] = '\0';
    if (filename != NULL) {
        (void)sixel_compat_snprintf(loader->log_path,
                                    sizeof(loader->log_path),
                                    "%s",
                                    filename);
    }

    memset(&callback_state, 0, sizeof(callback_state));
    callback_state.loader = loader;
    callback_state.fn = fn_load;
    callback_state.context = loader->context;
    loader->callback_failed = 0;

    reqcolors = loader->reqcolors;
    if (reqcolors > SIXEL_PALETTE_MAX) {
        reqcolors = SIXEL_PALETTE_MAX;
    }

    status = sixel_factory_get_default((void **)&factory);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = factory->vtbl->create(factory,
                                   "loader/manager",
                                   loader->allocator,
                                   (void **)&manager);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    osc11_timeout_env = sixel_compat_getenv(
        SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_ENV);
    osc11_timeout_ms = sixel_loader_parse_osc11_bg_query_timeout_ms(
        osc11_timeout_env);

    /*
     * Launch OSC11 probing before sixel_chunk_new() so the terminal roundtrip
     * overlaps with input loading. If thread creation is unavailable, fall
     * back to synchronous probing and keep failures non-fatal.
     */
    if (loader_can_query_osc11_bgcolor(loader) != 0) {
        osc11_query_job.timeout_ms = osc11_timeout_ms;
        thread_status = sixel_thread_create(
            &osc11_query_job.thread,
            loader_osc11_bg_query_thread_main,
            &osc11_query_job);
        if (SIXEL_SUCCEEDED(thread_status)) {
            osc11_query_job.started = 1;
        } else {
            osc11_query_job.status = sixel_tty_query_osc11_bgcolor(
                osc11_query_job.bgcolor,
                osc11_query_job.timeout_ms);
            if (SIXEL_SUCCEEDED(osc11_query_job.status)) {
                loader->bgcolor[0] = osc11_query_job.bgcolor[0];
                loader->bgcolor[1] = osc11_query_job.bgcolor[1];
                loader->bgcolor[2] = osc11_query_job.bgcolor[2];
                loader->has_bgcolor = 1;
                osc11_bgcolor_applied = 1;
            }
        }
    }

    chunk_job_id = loader_timeline_next_job(loader);
    loader_log_timeline_event(loader,
                              "loader/manager",
                              "chunk/create",
                              "start",
                              chunk_job_id);
    status = sixel_chunk_new(&pchunk,
                             filename,
                             loader->finsecure,
                             loader->cancel_flag,
                             loader->allocator);
    if (status != SIXEL_OK) {
        loader_log_timeline_event(loader,
                                  "loader/manager",
                                  "chunk/create",
                                  "fail",
                                  chunk_job_id);
        goto end;
    }
    loader_log_timeline_event(loader,
                              "loader/manager",
                              "chunk/create",
                              "finish",
                              chunk_job_id);

    if (pchunk->size == 0 || (pchunk->size == 1 && *pchunk->buffer == '\n')) {
        status = SIXEL_OK;
        goto end;
    }

    if (pchunk->source_path != NULL && pchunk->source_path[0] != '\0') {
        (void)sixel_compat_snprintf(loader->log_path,
                                    sizeof(loader->log_path),
                                    "%s",
                                    pchunk->source_path);
    }

    if (pchunk->buffer == NULL || pchunk->max_size == 0) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    status = SIXEL_FALSE;
    order_override = loader->loader_order;
    if (order_override == NULL) {
        env_order = sixel_compat_getenv("SIXEL_LOADER_PRIORITY_LIST");
        if (env_order != NULL && env_order[0] != '\0') {
            order_override = env_order;
        }
    }
    if (order_override != NULL && order_override[0] != '\0') {
        if (order_override == loader->loader_order) {
            active_order_resolution = &loader->loader_order_resolution;
        } else {
            status = loader_manager_parse_loader_order(order_override,
                                                       &order_resolution);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            active_order_resolution = &order_resolution;
        }
    }
    loader_manager_resolve_loader_suboptions(active_order_resolution,
                                             &active_suboptions);
    /*
     * Resolve OSC11 before build_chain so component options are finalized
     * once during chain construction.
     */
    if (osc11_query_job.started != 0 && loader->has_bgcolor == 0) {
        wait_result = loader_osc11_bg_query_job_is_finished(&osc11_query_job);
        if (wait_result == 0) {
            wait_result = sixel_loader_wait_for_condition(
                loader_osc11_bg_query_job_is_finished,
                &osc11_query_job,
                osc11_timeout_ms);
            (void)wait_result;
        }
        if (loader_osc11_bg_query_job_apply_if_ready(loader,
                                                     &osc11_query_job) != 0) {
            osc11_bgcolor_applied = 1;
        }
    }

    if (osc11_bgcolor_applied != 0) {
        /*
         * OSC11 replies are terminal UI colors and therefore gamma-encoded.
         * Force gamma interpretation for this load even when the process
         * default requested a different background color space.
         */
        sixel_helper_set_loader_background_colorspace(
            SIXEL_COLORSPACE_GAMMA);
    }

    build_request.resolution = active_order_resolution;
    build_request.require_static = loader->fstatic;
    build_request.use_palette = loader->fuse_palette;
    build_request.reqcolors = reqcolors;
    build_request.bgcolor = loader->bgcolor;
    build_request.has_bgcolor = loader->has_bgcolor;
    build_request.bgcolor_source = loader->bgcolor_source;
    build_request.loop_control = loader->loop_control;
    build_request.has_start_frame_no = loader->has_start_frame_no;
    build_request.start_frame_no = loader->start_frame_no;
    build_request.suboptions = &active_suboptions;
    build_request.timeline_logger = &loader->logger;
    build_request.timeline_job_seq = &loader->log_timeline_job_seq;
    build_request.timeline_loader = loader;
    build_request.skip_predicate_gate =
        (active_order_resolution != NULL &&
         active_order_resolution->has_trailing_bang &&
         active_order_resolution->item_count == 1u) ? 1 : 0;

    status = manager->vtbl->build_chain(manager, &build_request);
    if (SIXEL_FAILED(status)) {
        if (status == SIXEL_BAD_ARGUMENT &&
            active_order_resolution != NULL &&
            active_order_resolution->canonical_argument != NULL &&
            active_order_resolution->canonical_argument[0] != '\0') {
            sixel_helper_set_additional_message(
                "sixel_loader_load_file: no supported loader in loader "
                "order.");
            status = SIXEL_BAD_ARGUMENT;
        } else if (status == SIXEL_BAD_ARGUMENT) {
            sixel_helper_set_additional_message(
                "sixel_loader_load_file: no available loader backend.");
            status = SIXEL_LOADER_FAILED;
        }
        goto end;
    }

    loader_timeline_select_phase_start(loader,
                                       "loader/manager",
                                       &loader->timeline_manager_select_job,
                                       &loader->timeline_manager_select_open);
    status = manager->vtbl->load(
        manager,
        pchunk,
        NULL,
        loader_callback_trampoline,
        &callback_state);
    loader_timeline_select_phase_finish(
        loader,
        "loader/manager",
        &loader->timeline_manager_select_job,
        &loader->timeline_manager_select_open,
        SIXEL_SUCCEEDED(status) ? "finish" : "fail");

    if (SIXEL_FAILED(status)) {
        if (status == SIXEL_FALSE) {
            if (!loader->callback_failed && pchunk != NULL) {
                status = SIXEL_LOADER_FAILED;
                sixel_helper_set_additional_message(
                    "sixel_loader_load_file: no loader decoded input.");
            } else {
                sixel_helper_set_additional_message(
                    "sixel_loader_load_file: loader returned "
                    "unspecified failure.");
                status = SIXEL_LOADER_FAILED;
            }
        }
        goto end;
    }

end:
    if (osc11_bgcolor_applied != 0) {
        sixel_helper_set_loader_background_colorspace(-1);
    }
    loader_osc11_bg_query_job_join(&osc11_query_job);
    if (manager != NULL) {
        manager->vtbl->unref(manager);
    }
    manager = NULL;
    factory = NULL;
    sixel_chunk_destroy(pchunk);
    sixel_option_free_argument_list_resolution(&order_resolution);
    sixel_loader_unref(loader);

end0:
    return status;
}

/* load image from file */

SIXELAPI SIXELSTATUS
sixel_helper_load_image_file(
    char const                /* in */     *filename,     /* source file name */
    int                       /* in */     fstatic,       /* whether to */
                                                             /* extract a */
                                                             /* static image */
                                                             /* from an */
                                                             /* animated gif */
    int                       /* in */     fuse_palette,  /* whether to */
                                                             /* use a */
                                                             /* paletted */
                                                             /* image; set */
                                                             /* non-zero to */
                                                             /* request one */
    int                       /* in */     reqcolors,     /* requested */
                                                             /* number of */
                                                             /* colors; */
                                                             /* should be */
                                                             /* equal to or */
                                                             /* less than */
                                                             /* SIXEL_ */
                                                             /* PALETTE_ */
                                                             /* MAX */
    unsigned char             /* in */     *bgcolor,      /* background */
                                                             /* color, may */
                                                             /* be NULL */
    int                       /* in */     loop_control,  /* one of enum */
                                                             /* loopControl */
    sixel_load_image_function /* in */     fn_load,       /* callback */
    int                       /* in */     finsecure,     /* true if do */
                                                             /* not verify */
                                                             /* SSL */
    int const                 /* in */     *cancel_flag,  /* cancel flag, */
                                                             /* may be */
                                                             /* NULL */
    void                      /* in/out */ *context,      /* private data */
                                                             /* passed to */
                                                             /* callback */
                                                             /* function, */
                                                             /* may be */
                                                             /* NULL */
    sixel_allocator_t         /* in */     *allocator     /* allocator */
                                                             /* object, */
                                                             /* may be */
                                                             /* NULL */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_loader_t *loader;

    loader = NULL;

    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                 &fstatic);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_USE_PALETTE,
                                 &fuse_palette);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQCOLORS,
                                 &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_BGCOLOR,
                                 bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                 &loop_control);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_INSECURE,
                                 &finsecure);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CANCEL_FLAG,
                                 cancel_flag);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_load_file(loader, filename, fn_load);

end:
    sixel_loader_unref(loader);

    return status;
}


SIXELAPI size_t
sixel_helper_get_available_loader_names(char const **names, size_t max_names)
{
    sixel_loader_entry_t const *entries;
    size_t entry_count;
    size_t limit;
    size_t index;

    entries = NULL;
    entry_count = 0u;
    limit = 0u;
    index = 0u;
    entry_count = loader_manager_get_entries(&entries);

    if (names != NULL && max_names > 0) {
        limit = entry_count;
        if (limit > max_names) {
            limit = max_names;
        }
        for (index = 0; index < limit; ++index) {
            names[index] = entries[index].name;
        }
    }

    return entry_count;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
