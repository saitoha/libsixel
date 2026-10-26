/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LIBSIXEL_THREADING_H
#define LIBSIXEL_THREADING_H

#include <sixel.h>
#include "options.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*sixel_thread_fn)(void *arg);

struct sixel_runtime_policy_options;
struct sixel_diagnostics_policy_options;
struct sixel_clipboard_policy_options;

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
#include <windows.h>

typedef DWORD sixel_thread_identity_t;

struct sixel_mutex {
    CRITICAL_SECTION native;
};

struct sixel_cond {
    CONDITION_VARIABLE native;
};

struct sixel_thread {
    HANDLE handle;         /* OS thread handle */
    sixel_thread_fn fn;    /* user callback */
    void *arg;             /* user data */
    int result;            /* callback result */
    int started;           /* thread start flag */
};
#else
#include <pthread.h>

typedef pthread_t sixel_thread_identity_t;

struct sixel_mutex {
    pthread_mutex_t native;
};

struct sixel_cond {
    pthread_cond_t native;
};

struct sixel_thread {
    pthread_t handle;
    sixel_thread_fn fn;
    void *arg;
    int result;
    int started;
};
#endif

typedef struct sixel_mutex sixel_mutex_t;
typedef struct sixel_cond sixel_cond_t;
typedef struct sixel_thread sixel_thread_t;

/* Identities are compared only while their owning operation is alive. */
SIXEL_INTERNAL_API sixel_thread_identity_t sixel_thread_identity_current(void);
SIXEL_INTERNAL_API int sixel_thread_identity_equal(
    sixel_thread_identity_t left, sixel_thread_identity_t right);

SIXEL_INTERNAL_API int sixel_mutex_init(sixel_mutex_t *mutex);
SIXEL_INTERNAL_API void sixel_mutex_destroy(sixel_mutex_t *mutex);
SIXEL_INTERNAL_API void sixel_mutex_lock(sixel_mutex_t *mutex);
SIXEL_INTERNAL_API void sixel_mutex_unlock(sixel_mutex_t *mutex);

SIXEL_INTERNAL_API int sixel_cond_init(sixel_cond_t *cond);
SIXEL_INTERNAL_API void sixel_cond_destroy(sixel_cond_t *cond);
SIXEL_INTERNAL_API void sixel_cond_wait(sixel_cond_t *cond,
                                        sixel_mutex_t *mutex);
/* Wait at most TIMEOUT_MS milliseconds. Returns nonzero on timeout; like
 * sixel_cond_wait(), a zero return may also be a spurious wakeup. */
SIXEL_INTERNAL_API int sixel_cond_timedwait(sixel_cond_t *cond,
                                            sixel_mutex_t *mutex,
                                            int timeout_ms);
SIXEL_INTERNAL_API void sixel_cond_signal(sixel_cond_t *cond);
SIXEL_INTERNAL_API void sixel_cond_broadcast(sixel_cond_t *cond);

SIXEL_INTERNAL_API int sixel_thread_create(sixel_thread_t *thread,
                                 sixel_thread_fn fn,
                                 void *arg);
SIXEL_INTERNAL_API void sixel_thread_join(sixel_thread_t *thread);
SIXEL_INTERNAL_API int sixel_thread_pin_self(int cpu_index);
SIXEL_INTERNAL_API int sixel_get_hw_threads(void);
/* Heterogeneous CPUs expose only performance logical CPUs; failures return
 * the complete hardware count, preserving homogeneous/VM behavior. */
SIXEL_INTERNAL_API int sixel_get_performance_threads(void);

/* A work entry resolves environment and API defaults once. Copies passed to
 * helper threads preserve mode, limits and frame keys for the whole run. */
typedef struct sixel_threading_snapshot {
    sixel_threading_policy_options_t options;
    int budget;
    int explicit_budget;
    int decoder;
    /* Optional session work accounting. Callbacks run outside pool/config
     * locks. A worker inherits the hook, but never a controller's claim. */
    void (*work)(void *, int, int);
    void *work_context;
    int work_worker;
} sixel_threading_snapshot_t;

SIXEL_INTERNAL_API void sixel_threading_policy_resolve(
    sixel_threading_policy_options_t const *options, int decoder,
    sixel_threading_snapshot_t *snapshot);
/* Return true for an active scope; otherwise resolve the specified side. */
SIXEL_INTERNAL_API int sixel_threading_policy_capture(
    sixel_threading_snapshot_t *snapshot, int decoder);

/* Stack-owned scopes are registered by native thread identity under the
 * config mutex, avoiding compiler-specific TLS and allocation failures.
 * Budget scopes inherit their parent; a policy entry starts an independent
 * operation, so a nested call on a different object keeps its own budget. */
typedef struct sixel_thread_budget_scope {
    sixel_thread_identity_t owner;
    sixel_threading_snapshot_t policy;
    int policy_root;
    int threads;
    int ceiling;
    int (*reclaim)(void *);
    void *reclaim_context;
    struct sixel_thread_budget_scope *next;
} sixel_thread_budget_scope_t;

SIXEL_INTERNAL_API void sixel_threading_policy_enter(
    sixel_thread_budget_scope_t *scope,
    sixel_threading_snapshot_t const *snapshot);
SIXEL_INTERNAL_API void sixel_threading_object_enter(
    sixel_thread_budget_scope_t *scope,
    sixel_threading_policy_options_t const *options, int decoder);
SIXEL_INTERNAL_API void sixel_thread_budget_enter(
    sixel_thread_budget_scope_t *scope, int threads);
SIXEL_INTERNAL_API void sixel_thread_budget_leave(
    sixel_thread_budget_scope_t *scope);

/* A reclaim callback runs only on the scope owner, outside the configuration
 * lock. It must return true only after all donated execution slots are idle.
 * The context must outlive the scope; polling must not wait for active work. */
SIXEL_INTERNAL_API void sixel_thread_budget_set_reclaim(
    sixel_thread_budget_scope_t *scope, int ceiling,
    int (*reclaim)(void *), void *context);
/* Cooperative job boundaries. Idle callers release their execution claim
 * before waiting for children; only background begin callbacks may wait. */
SIXEL_INTERNAL_API void sixel_thread_work_begin(void);
SIXEL_INTERNAL_API void sixel_thread_work_end(void);
SIXEL_INTERNAL_API void sixel_thread_work_checkpoint(void);
SIXEL_INTERNAL_API int sixel_threads_ceiling(void);
SIXEL_INTERNAL_API int sixel_threads_reclaim(void);

SIXEL_INTERNAL_API int sixel_threads_normalize(int requested);
SIXEL_INTERNAL_API int sixel_threads_resolve(void);
/* Data-parallel stages stay serial in lazy mode. Pipeline totals do not. */
SIXEL_INTERNAL_API int sixel_threads_resolve_fanout(void);
SIXEL_INTERNAL_API int sixel_threading_policy_mode(void);
SIXEL_INTERNAL_API char const *sixel_threading_policy_mode_name(void);

struct sixel_threading_policy_options;

typedef enum sixel_threading_stage {
    SIXEL_THREADING_STAGE_DITHER = 0,
    SIXEL_THREADING_STAGE_ENCODE,
    SIXEL_THREADING_STAGE_PREPARE,
    SIXEL_THREADING_STAGE_DECODE
} sixel_threading_stage_t;

SIXEL_INTERNAL_API void sixel_threading_policy_load(
    struct sixel_threading_policy_options *options);
/* Explicit means a CLI/API/environment budget or a selected base mode.
 * The returned budget includes the balanced performance-core cap. */
SIXEL_INTERNAL_API int sixel_threading_policy_explicit_budget(int *threads);
/* Frame-level modes return sixel_threading_frame_mode_t values. */
SIXEL_INTERNAL_API int sixel_threading_policy_load_mode(void);
SIXEL_INTERNAL_API int sixel_threading_policy_prepare_mode(void);
/* Requested palette mode from the frozen scope, not execution eligibility.
 * Latest currently resolves to the existing frame execution path. */
SIXEL_INTERNAL_API int sixel_threading_policy_palette_mode(void);
SIXEL_INTERNAL_API int sixel_threading_policy_load_depth(void);
/* Returns true with an explicit stage limit; the caller fits it to the
 * resolved budget. */
SIXEL_INTERNAL_API int sixel_threading_policy_stage_threads(
    sixel_threading_stage_t stage, unsigned int *count);
SIXEL_INTERNAL_API int sixel_threading_policy_pin(int fallback);

SIXEL_INTERNAL_API void sixel_runtime_policy_load(
    struct sixel_runtime_policy_options *options);
SIXEL_INTERNAL_API void sixel_runtime_policy_store(
    struct sixel_runtime_policy_options const *options);
SIXEL_INTERNAL_API int sixel_runtime_policy_simd_level(int fallback);
SIXEL_INTERNAL_API size_t sixel_runtime_policy_sample_min_pixels(
    size_t fallback);
SIXEL_INTERNAL_API size_t sixel_runtime_policy_colorspace_min_pixels(
    size_t fallback);
SIXEL_INTERNAL_API unsigned int sixel_runtime_policy_parallel_factor(
    unsigned int fallback);
SIXEL_INTERNAL_API int sixel_runtime_policy_parallel_skew(int fallback);
SIXEL_INTERNAL_API int sixel_runtime_policy_resize_precision(int fallback);
SIXEL_INTERNAL_API size_t sixel_runtime_policy_scale_min_bytes(
    size_t fallback);
SIXEL_INTERNAL_API void sixel_diagnostics_policy_load(
    struct sixel_diagnostics_policy_options *options);
SIXEL_INTERNAL_API int sixel_diagnostics_policy_store(
    struct sixel_diagnostics_policy_options const *options);
SIXEL_INTERNAL_API void sixel_clipboard_policy_load(
    struct sixel_clipboard_policy_options *options);
SIXEL_INTERNAL_API int sixel_clipboard_policy_store(
    struct sixel_clipboard_policy_options const *options);
SIXEL_INTERNAL_API int sixel_clipboard_policy_copy_directory_override(
    char *buffer,
    size_t buffer_size,
    int *configured);
SIXEL_INTERNAL_API int sixel_diagnostics_topic_list_contains(
    char const *topics,
    char const *topic);
SIXEL_INTERNAL_API int sixel_diagnostics_policy_trace_topic_is_enabled(
    char const *topic,
    int *configured);
SIXEL_INTERNAL_API void
sixel_diagnostics_policy_enable_cli_suggestion_defaults(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_THREADING_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
