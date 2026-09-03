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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(__linux__) && !defined(_GNU_SOURCE)
# define _GNU_SOURCE
#endif

/*
 * OpenBSD hides BSD typedefs such as u_long when _POSIX_C_SOURCE is defined.
 * Enable the BSD namespace locally so sys/sysctl.h exposes the kernel
 * structures needed for hardware concurrency detection.
 */
#if defined(__OpenBSD__)
# define _BSD_SOURCE
#endif

/*
 * NetBSD also conceals legacy typedefs like devmajor_t and u_int under
 * strict POSIX feature sets. Force the wider namespace so sys/sysctl.h can
 * provide the structures required by our concurrency probing helpers.
 */
#if defined(__NetBSD__)
# define _NETBSD_SOURCE
#endif

/*
 * for including sys/sysctl.h on DragonflyBSD
 */
#if defined(__DragonFly__)
# define _DRAGONFLY_SOURCE
#endif

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
/*
 * Expose BSD-flavoured typedefs such as u_int from the macOS SDK when the
 * build defines _POSIX_C_SOURCE. The platform headers hide these legacy names
 * otherwise, and sys/sysctl.h requires them for data structures like
 * struct kinfo_proc.
 */
# define _DARWIN_C_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if defined(__APPLE__)
# if HAVE_SYS_SYSCTL_H
#  include <sys/sysctl.h>
# endif
#endif

#include "compat_stub.h"
#include "threading.h"
#include "options.h"
#include "options-registry.h"

/*
 * Backend selection is performed here so the header remains lightweight.
 * WITH_WINPTHREAD forces the pthread path even on Windows to honor
 * user-provided configuration switches.
 */
#if defined(WITH_WINPTHREAD) && WITH_WINPTHREAD
# define SIXEL_USE_PTHREADS 1
# define SIXEL_USE_WIN32_THREADS 0
#elif defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)
# define SIXEL_USE_PTHREADS 0
# define SIXEL_USE_WIN32_THREADS 1
#elif SIXEL_ENABLE_THREADS
# define SIXEL_USE_PTHREADS 1
# define SIXEL_USE_WIN32_THREADS 0
#else
# define SIXEL_USE_PTHREADS 0
# define SIXEL_USE_WIN32_THREADS 0
#endif

#if SIXEL_USE_WIN32_THREADS
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
# include <process.h>
#endif

/*
 * Provide a thin portability layer for synchronization primitives so the
 * encoder can run on POSIX and Windows platforms without altering the public
 * API surface.
 */

#if SIXEL_USE_PTHREADS
/*
 * Abort the process when a pthread call fails in a context where recovery is
 * impossible. Encoding without locking guarantees would corrupt state, so we
 * surface an explicit diagnostic before terminating.
 */
static void
sixel_pthread_abort(const char *what, int error)
{
    fprintf(stderr, "libsixel: %s failed: %d\n", what, error);
    abort();
}

/*
 * Entry point passed to pthread_create. It forwards execution to the user
 * supplied callback and stores the integer status for later inspection.
 */
static void *
sixel_thread_trampoline(void *arg)
{
    sixel_thread_t *thread;

    thread = (sixel_thread_t *)arg;
    thread->result = thread->fn(thread->arg);
    return NULL;
}

SIXELAPI int
sixel_mutex_init(sixel_mutex_t *mutex)
{
    int rc;

    if (mutex == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    /*
     * Default attributes already provide a non-recursive mutex, which is
     * sufficient for the encoder's synchronization requirements.
     */
    rc = pthread_mutex_init(&mutex->native, NULL);
    if (rc != 0) {
        errno = rc;
        return SIXEL_RUNTIME_ERROR;
    }
    return SIXEL_OK;
}

SIXELAPI void
sixel_mutex_destroy(sixel_mutex_t *mutex)
{
    int rc;

    if (mutex == NULL) {
        return;
    }
    rc = pthread_mutex_destroy(&mutex->native);
    if (rc != 0) {
        sixel_pthread_abort("pthread_mutex_destroy", rc);
    }
}

SIXELAPI void
sixel_mutex_lock(sixel_mutex_t *mutex)
{
    int rc;

    if (mutex == NULL) {
        return;
    }
    rc = pthread_mutex_lock(&mutex->native);
    if (rc != 0) {
        sixel_pthread_abort("pthread_mutex_lock", rc);
    }
}

SIXELAPI void
sixel_mutex_unlock(sixel_mutex_t *mutex)
{
    int rc;

    if (mutex == NULL) {
        return;
    }
    rc = pthread_mutex_unlock(&mutex->native);
    if (rc != 0) {
        sixel_pthread_abort("pthread_mutex_unlock", rc);
    }
}

SIXELAPI int
sixel_cond_init(sixel_cond_t *cond)
{
    int rc;

    if (cond == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    /*
     * Conditions wake waiters in FIFO order per pthreads documentation. No
     * custom attributes are needed for the thread pool queue.
     */
    rc = pthread_cond_init(&cond->native, NULL);
    if (rc != 0) {
        errno = rc;
        return SIXEL_RUNTIME_ERROR;
    }
    return SIXEL_OK;
}

SIXELAPI void
sixel_cond_destroy(sixel_cond_t *cond)
{
    int rc;

    if (cond == NULL) {
        return;
    }
    rc = pthread_cond_destroy(&cond->native);
    if (rc != 0) {
        sixel_pthread_abort("pthread_cond_destroy", rc);
    }
}

SIXELAPI void
sixel_cond_wait(sixel_cond_t *cond, sixel_mutex_t *mutex)
{
    int rc;

    if (cond == NULL || mutex == NULL) {
        return;
    }
    /*
     * Some libc/pthread implementations may report EINTR when a signal is
     * delivered while waiting. Retry so cooperative cancellation paths can
     * finish via shared state updates instead of aborting the process.
     */
#if HAVE_ERRNO_H
    do {
        rc = pthread_cond_wait(&cond->native, &mutex->native);
    } while (rc == EINTR);
#else
    rc = pthread_cond_wait(&cond->native, &mutex->native);
#endif
    if (rc != 0) {
        sixel_pthread_abort("pthread_cond_wait", rc);
    }
}

SIXELAPI void
sixel_cond_signal(sixel_cond_t *cond)
{
    int rc;

    if (cond == NULL) {
        return;
    }
    rc = pthread_cond_signal(&cond->native);
    if (rc != 0) {
        sixel_pthread_abort("pthread_cond_signal", rc);
    }
}

SIXELAPI void
sixel_cond_broadcast(sixel_cond_t *cond)
{
    int rc;

    if (cond == NULL) {
        return;
    }
    rc = pthread_cond_broadcast(&cond->native);
    if (rc != 0) {
        sixel_pthread_abort("pthread_cond_broadcast", rc);
    }
}

SIXELAPI int
sixel_thread_create(sixel_thread_t *thread, sixel_thread_fn fn, void *arg)
{
    int rc;

    if (thread == NULL || fn == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    /*
     * Store context before launching so the trampoline can record the
     * callback result inside the same structure without extra allocations.
     */
    thread->fn = fn;
    thread->arg = arg;
    thread->result = SIXEL_OK;
    thread->started = 0;
    rc = pthread_create(&thread->handle, NULL, sixel_thread_trampoline,
                        thread);
    if (rc != 0) {
        errno = rc;
        return SIXEL_RUNTIME_ERROR;
    }
    thread->started = 1;
    return SIXEL_OK;
}

SIXELAPI void
sixel_thread_join(sixel_thread_t *thread)
{
    int rc;

    if (thread == NULL || !thread->started) {
        return;
    }
    rc = pthread_join(thread->handle, NULL);
    if (rc != 0) {
        sixel_pthread_abort("pthread_join", rc);
    }
    thread->started = 0;
}

/*
 * Best-effort CPU affinity setter for the calling thread. Platforms without
 * pthread_setaffinity_np simply ignore the request so callers can use the
 * helper unconditionally.
 */
SIXELAPI int
sixel_thread_pin_self(int cpu_index)
{
#if defined(__linux__) && defined(CPU_SET)
    cpu_set_t set;
    int rc;

    if (cpu_index < 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    CPU_ZERO(&set);
    CPU_SET((unsigned int)cpu_index, &set);
    rc = pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    if (rc != 0) {
        return SIXEL_RUNTIME_ERROR;
    }

    return SIXEL_OK;
#else
    (void)cpu_index;

    return SIXEL_OK;
#endif
}

SIXELAPI int
sixel_get_hw_threads(void)
{
#if defined(_SC_NPROCESSORS_ONLN)
    long count;

    count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count > 0 && count <= (long)INT_MAX) {
        return (int)count;
    }
#endif
#if defined(__APPLE__)
    {
        int mib[2];
        size_t size;
        int value;

        mib[0] = CTL_HW;
        mib[1] = HW_AVAILCPU;
        size = sizeof(value);
        if (sysctl(mib, 2, &value, &size, NULL, 0) == 0 && value > 0) {
            return value;
        }
        mib[1] = HW_NCPU;
        size = sizeof(value);
        if (sysctl(mib, 2, &value, &size, NULL, 0) == 0 && value > 0) {
            return value;
        }
    }
#endif
    return 1;
}

#elif SIXEL_USE_WIN32_THREADS

/*
 * Abort execution on unrecoverable Win32 API failures to mirror pthread path
 * semantics. Printing the failing call and error code helps debugging in
 * environments where stderr is available.
 */
static void
sixel_win32_abort(const char *what, DWORD error)
{
    fprintf(stderr, "libsixel: %s failed: %lu\n", what,
            (unsigned long)error);
    abort();
}

/*
 * Trampoline for _beginthreadex. It records the callback result back into the
 * owning sixel_thread_t structure so the caller can retrieve it after join.
 */
static unsigned __stdcall
sixel_win32_thread_start(void *arg)
{
    sixel_thread_t *thread;

    thread = (sixel_thread_t *)arg;
    thread->result = thread->fn(thread->arg);
    return 0;
}

SIXELAPI int
sixel_mutex_init(sixel_mutex_t *mutex)
{
    if (mutex == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    InitializeCriticalSection(&mutex->native);
    return SIXEL_OK;
}

SIXELAPI void
sixel_mutex_destroy(sixel_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }
    DeleteCriticalSection(&mutex->native);
}

SIXELAPI void
sixel_mutex_lock(sixel_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }
    EnterCriticalSection(&mutex->native);
}

SIXELAPI void
sixel_mutex_unlock(sixel_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }
    LeaveCriticalSection(&mutex->native);
}

SIXELAPI int
sixel_cond_init(sixel_cond_t *cond)
{
    if (cond == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    InitializeConditionVariable(&cond->native);
    return SIXEL_OK;
}

SIXELAPI void
sixel_cond_destroy(sixel_cond_t *cond)
{
    /* CONDITION_VARIABLE does not need explicit teardown. */
    (void)cond;
}

SIXELAPI void
sixel_cond_wait(sixel_cond_t *cond, sixel_mutex_t *mutex)
{
    BOOL rc;
    DWORD error;

    if (cond == NULL || mutex == NULL) {
        return;
    }
    rc = SleepConditionVariableCS(&cond->native, &mutex->native, INFINITE);
    if (rc == 0) {
        error = GetLastError();
        sixel_win32_abort("SleepConditionVariableCS", error);
    }
}

SIXELAPI void
sixel_cond_signal(sixel_cond_t *cond)
{
    if (cond == NULL) {
        return;
    }
    WakeConditionVariable(&cond->native);
}

SIXELAPI void
sixel_cond_broadcast(sixel_cond_t *cond)
{
    if (cond == NULL) {
        return;
    }
    WakeAllConditionVariable(&cond->native);
}

SIXELAPI int
sixel_thread_create(sixel_thread_t *thread, sixel_thread_fn fn, void *arg)
{
    uintptr_t handle;

    if (thread == NULL || fn == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    thread->fn = fn;
    thread->arg = arg;
    thread->result = SIXEL_OK;
    thread->started = 0;
    handle = _beginthreadex(NULL, 0, sixel_win32_thread_start, thread, 0,
                            NULL);
    if (handle == 0) {
        return SIXEL_RUNTIME_ERROR;
    }
    thread->handle = (HANDLE)handle;
    thread->started = 1;
    return SIXEL_OK;
}

SIXELAPI void
sixel_thread_join(sixel_thread_t *thread)
{
    DWORD rc;
    DWORD error;

    if (thread == NULL || !thread->started) {
        return;
    }
    rc = WaitForSingleObject(thread->handle, INFINITE);
    if (rc != WAIT_OBJECT_0) {
        error = (rc == WAIT_FAILED) ? GetLastError() : rc;
        sixel_win32_abort("WaitForSingleObject", error);
    }
    CloseHandle(thread->handle);
    thread->handle = NULL;
    thread->started = 0;
}

SIXELAPI int
sixel_thread_pin_self(int cpu_index)
{
    DWORD_PTR mask;
    DWORD_PTR previous;

    if (cpu_index < 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    mask = ((DWORD_PTR)1) << (DWORD_PTR)cpu_index;
    previous = SetThreadAffinityMask(GetCurrentThread(), mask);
    if (previous == 0) {
        return SIXEL_RUNTIME_ERROR;
    }

    return SIXEL_OK;
}

SIXELAPI int
sixel_get_hw_threads(void)
{
    DWORD count;
    SYSTEM_INFO info;

    count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (count == 0) {
        GetSystemInfo(&info);
        count = info.dwNumberOfProcessors;
    }
    if (count == 0) {
        count = 1;
    }
    return (int)count;
}

#else
/*
 * Thread support is disabled. Provide stub implementations so callers that
 * inadvertently use the API receive a deterministic failure rather than a
 * linker error. Mutex and condition helpers become no-ops while creation
 * attempts return an explicit runtime error.
 */
SIXELAPI int
sixel_mutex_init(sixel_mutex_t *mutex)
{
    (void)mutex;
    return SIXEL_RUNTIME_ERROR;
}

SIXELAPI void
sixel_mutex_destroy(sixel_mutex_t *mutex)
{
    (void)mutex;
}

SIXELAPI void
sixel_mutex_lock(sixel_mutex_t *mutex)
{
    (void)mutex;
}

SIXELAPI void
sixel_mutex_unlock(sixel_mutex_t *mutex)
{
    (void)mutex;
}

SIXELAPI int
sixel_cond_init(sixel_cond_t *cond)
{
    (void)cond;
    return SIXEL_RUNTIME_ERROR;
}

SIXELAPI void
sixel_cond_destroy(sixel_cond_t *cond)
{
    (void)cond;
}

SIXELAPI void
sixel_cond_wait(sixel_cond_t *cond, sixel_mutex_t *mutex)
{
    (void)cond;
    (void)mutex;
}

SIXELAPI void
sixel_cond_signal(sixel_cond_t *cond)
{
    (void)cond;
}

SIXELAPI void
sixel_cond_broadcast(sixel_cond_t *cond)
{
    (void)cond;
}

SIXELAPI int
sixel_thread_create(sixel_thread_t *thread, sixel_thread_fn fn, void *arg)
{
    (void)thread;
    (void)fn;
    (void)arg;
    return SIXEL_RUNTIME_ERROR;
}

SIXELAPI void
sixel_thread_join(sixel_thread_t *thread)
{
    (void)thread;
}

SIXELAPI int
sixel_thread_pin_self(int cpu_index)
{
    (void)cpu_index;

    return SIXEL_OK;
}

SIXELAPI int
sixel_get_hw_threads(void)
{
    return 1;
}

#endif /* SIXEL_USE_PTHREADS */

#define SIXEL_PROCESSING_THREADS_DEFAULT 2

/*
 * Thread configuration keeps the precedence rules centralized:
 *   1. Library callers may override via `sixel_set_threads`.
 *   2. Otherwise, `SIXEL_THREADS` from the environment is honored.
 *   3. Fallback gives encoder/dequantize paths a minimal two-stage pipeline.
 *
 * Direct SIXEL parsing has its own resolver in decoder-parallel.c so decoding
 * can stay serial by default while image post-processing still gets two slots.
 */
typedef struct sixel_thread_config_state {
    int requested_threads;
    int override_active;
} sixel_thread_config_state_t;

static sixel_thread_config_state_t g_thread_config = {
    SIXEL_PROCESSING_THREADS_DEFAULT,
    0
};

static sixel_runtime_policy_options_t g_runtime_policy;
static sixel_diagnostics_policy_options_t g_diagnostics_policy;

#if SIXEL_ENABLE_THREADS
static sixel_mutex_t g_thread_config_mutex;
static int g_thread_config_mutex_ready;

# if SIXEL_USE_WIN32_THREADS
static INIT_ONCE g_thread_config_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
sixel_thread_config_lock_init_once(PINIT_ONCE once,
                                   PVOID parameter,
                                   PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;

    if (sixel_mutex_init(&g_thread_config_mutex) == SIXEL_OK) {
        g_thread_config_mutex_ready = 1;
    }
    return TRUE;
}
# elif SIXEL_USE_PTHREADS
static pthread_once_t g_thread_config_once = PTHREAD_ONCE_INIT;

static void
sixel_thread_config_lock_init_once(void)
{
    if (sixel_mutex_init(&g_thread_config_mutex) == SIXEL_OK) {
        g_thread_config_mutex_ready = 1;
    }
}
# endif

static void
sixel_thread_config_lock(void)
{
# if SIXEL_USE_WIN32_THREADS
    BOOL initialized;

    initialized = InitOnceExecuteOnce(&g_thread_config_once,
                                      sixel_thread_config_lock_init_once,
                                      NULL,
                                      NULL);
    if (!initialized || !g_thread_config_mutex_ready) {
        abort();
    }
    sixel_mutex_lock(&g_thread_config_mutex);
# elif SIXEL_USE_PTHREADS
    int once_status;

    once_status = pthread_once(&g_thread_config_once,
                               sixel_thread_config_lock_init_once);
    if (once_status != 0 || !g_thread_config_mutex_ready) {
        abort();
    }
    sixel_mutex_lock(&g_thread_config_mutex);
# endif
}

static void
sixel_thread_config_unlock(void)
{
    if (!g_thread_config_mutex_ready) {
        abort();
    }
    sixel_mutex_unlock(&g_thread_config_mutex);
}
#else
static void
sixel_thread_config_lock(void)
{
}

static void
sixel_thread_config_unlock(void)
{
}
#endif

SIXELAPI int
sixel_threads_normalize(int requested)
{
    int normalized;

#if SIXEL_ENABLE_THREADS
    int hw_threads;

    if (requested <= 0) {
        hw_threads = sixel_get_hw_threads();
        if (hw_threads < 1) {
            hw_threads = 1;
        }
        normalized = hw_threads;
    } else {
        normalized = requested;
    }

    if (normalized < 1) {
        normalized = 1;
    }
#else
    (void)requested;
    normalized = 1;
#endif

    return normalized;
}

#if SIXEL_ENABLE_THREADS
static int
sixel_threads_resolve_default(void)
{
    return sixel_threads_normalize(SIXEL_PROCESSING_THREADS_DEFAULT);
}

static int
sixel_threads_resolve_env(void)
{
    sixel_suboption_value_t value;

    memset(&value, 0, sizeof(value));
    if (sixel_option_resolve_scalar_environment(
            SIXEL_OPTION_SCHEMA_THREADS,
            &value,
            NULL,
            0u) == SIXEL_OPTION_ENVIRONMENT_MATCH) {
        return sixel_threads_normalize(value.int_value);
    }

    return sixel_threads_resolve_default();
}
#endif

SIXELAPI int
sixel_threads_resolve(void)
{
    int resolved;

    resolved = 1;
    sixel_thread_config_lock();
#if SIXEL_ENABLE_THREADS
    if (g_thread_config.override_active) {
        resolved = g_thread_config.requested_threads;
    } else {
        resolved = sixel_threads_resolve_env();
    }
#else
    resolved = 1;
#endif
    sixel_thread_config_unlock();

    return resolved;
}

/*
 * Public setter so CLI/bindings may override the runtime thread preference.
 */
SIXELAPI void
sixel_set_threads(int threads)
{
    sixel_thread_config_lock();
#if SIXEL_ENABLE_THREADS
    g_thread_config.requested_threads = sixel_threads_normalize(threads);
#else
    (void)threads;
    g_thread_config.requested_threads = 1;
#endif
    g_thread_config.override_active = 1;
    sixel_thread_config_unlock();
}

void
sixel_runtime_policy_load(sixel_runtime_policy_options_t *options)
{
    if (options == NULL) {
        return;
    }
    sixel_thread_config_lock();
    *options = g_runtime_policy;
    sixel_thread_config_unlock();
}

void
sixel_runtime_policy_store(sixel_runtime_policy_options_t const *options)
{
    if (options == NULL) {
        return;
    }
    sixel_thread_config_lock();
    g_runtime_policy = *options;
    sixel_thread_config_unlock();
}

void
sixel_diagnostics_policy_load(
    sixel_diagnostics_policy_options_t *options)
{
    if (options == NULL) {
        return;
    }
    sixel_thread_config_lock();
    *options = g_diagnostics_policy;
    options->trace_topic = NULL;
    sixel_thread_config_unlock();
}

static char *
sixel_diagnostics_policy_duplicate_string(char const *text)
{
    char *copy;
    size_t length;

    copy = NULL;
    length = 0u;
    if (text == NULL || text[0] == '\0') {
        return NULL;
    }
    length = strlen(text) + 1u;
    copy = (char *)malloc(length);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length);
    return copy;
}

int
sixel_diagnostics_policy_store(
    sixel_diagnostics_policy_options_t const *options)
{
    char *trace_topic_copy;
    char *old_trace_topic;

    trace_topic_copy = NULL;
    old_trace_topic = NULL;
    if (options == NULL) {
        return 0;
    }
    if (options->trace_topic_override) {
        trace_topic_copy = sixel_diagnostics_policy_duplicate_string(
            options->trace_topic);
        if (trace_topic_copy == NULL) {
            return 0;
        }
    }
    sixel_thread_config_lock();
    old_trace_topic = (char *)g_diagnostics_policy.trace_topic;
    g_diagnostics_policy = *options;
    g_diagnostics_policy.trace_topic = trace_topic_copy;
    sixel_thread_config_unlock();
    free(old_trace_topic);
    return 1;
}

int
sixel_diagnostics_topic_list_contains(char const *topics,
                                      char const *topic)
{
    char const *cursor;
    char const *token_end;
    size_t topic_length;
    size_t token_length;

    cursor = NULL;
    token_end = NULL;
    topic_length = 0u;
    token_length = 0u;
    if (topics == NULL || topics[0] == '\0' ||
        topic == NULL || topic[0] == '\0') {
        return 0;
    }
    topic_length = strlen(topic);
    cursor = topics;
    while (*cursor != '\0') {
        while (*cursor != '\0' &&
               (*cursor == ' ' || *cursor == '\t' || *cursor == ',' ||
                *cursor == ':' || *cursor == ';')) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        token_end = cursor;
        while (*token_end != '\0' &&
               *token_end != ' ' && *token_end != '\t' &&
               *token_end != ',' && *token_end != ':' &&
               *token_end != ';') {
            ++token_end;
        }
        token_length = (size_t)(token_end - cursor);
        if (token_length == topic_length &&
            strncmp(cursor, topic, token_length) == 0) {
            return 1;
        }
        cursor = token_end;
    }
    return 0;
}

int
sixel_diagnostics_policy_trace_topic_is_enabled(char const *topic,
                                                int *configured)
{
    int enabled;

    enabled = 0;
    if (configured == NULL) {
        return 0;
    }
    sixel_thread_config_lock();
    *configured = g_diagnostics_policy.trace_topic_override != 0;
    if (*configured) {
        enabled = sixel_diagnostics_topic_list_contains(
            g_diagnostics_policy.trace_topic,
            topic);
    }
    sixel_thread_config_unlock();
    return enabled;
}

void
sixel_diagnostics_policy_enable_cli_suggestion_defaults(void)
{
    sixel_thread_config_lock();
    g_diagnostics_policy.cli_suggestion_defaults = 1;
    sixel_thread_config_unlock();
}

int
sixel_runtime_policy_simd_level(int fallback)
{
    sixel_runtime_policy_options_t options;
    sixel_suboption_value_t parsed;
    int value;

    memset(&options, 0, sizeof(options));
    memset(&parsed, 0, sizeof(parsed));
    value = fallback;
    sixel_runtime_policy_load(&options);
    if (options.simd_level_override) {
        return options.simd_level;
    }
    if (sixel_option_resolve_scalar_environment(
            SIXEL_OPTION_SCHEMA_RUNTIME_POLICY,
            &parsed,
            NULL,
            0u) == SIXEL_OPTION_ENVIRONMENT_MATCH) {
        value = parsed.int_value;
    }
    return value;
}

size_t
sixel_runtime_policy_colorspace_min_pixels(size_t fallback)
{
    sixel_runtime_policy_options_t options;
    sixel_suboption_value_t value;

    memset(&options, 0, sizeof(options));
    memset(&value, 0, sizeof(value));
    sixel_runtime_policy_load(&options);
    if (options.colorspace_parallel_min_pixels_override) {
        return options.colorspace_parallel_min_pixels;
    }
    if (sixel_option_registry_resolve_runtime_binding(
            SIXEL_SUBOPTION_BINDING_ID_2(
                colorspace_parallel_min_pixels,
                colorspace_parallel_min_pixels_override),
            SIXEL_SUBOPTION_VALUE_SIZE,
            &value) == SIXEL_OPTION_ENVIRONMENT_MATCH) {
        return value.size_value;
    }
    return fallback;
}

unsigned int
sixel_runtime_policy_parallel_factor(unsigned int fallback)
{
    sixel_runtime_policy_options_t options;
    sixel_suboption_value_t value;

    memset(&options, 0, sizeof(options));
    memset(&value, 0, sizeof(value));
    sixel_runtime_policy_load(&options);
    if (options.parallel_factor_override) {
        return options.parallel_factor;
    }
    if (sixel_option_registry_resolve_runtime_binding(
            SIXEL_SUBOPTION_BINDING_ID_2(
                parallel_factor,
                parallel_factor_override),
            SIXEL_SUBOPTION_VALUE_UINT,
            &value) == SIXEL_OPTION_ENVIRONMENT_MATCH) {
        return value.uint_value;
    }
    return fallback;
}

int
sixel_runtime_policy_parallel_skew(int fallback)
{
    sixel_runtime_policy_options_t options;
    sixel_suboption_value_t value;

    memset(&options, 0, sizeof(options));
    memset(&value, 0, sizeof(value));
    sixel_runtime_policy_load(&options);
    if (options.parallel_skew_override) {
        return options.parallel_skew;
    }
    if (sixel_option_registry_resolve_runtime_binding(
            SIXEL_SUBOPTION_BINDING_ID_2(
                parallel_skew,
                parallel_skew_override),
            SIXEL_SUBOPTION_VALUE_INT,
            &value) == SIXEL_OPTION_ENVIRONMENT_MATCH) {
        return value.int_value;
    }
    return fallback;
}

int
sixel_runtime_policy_resize_precision(int fallback)
{
    sixel_runtime_policy_options_t options;
    sixel_suboption_value_t value;

    memset(&options, 0, sizeof(options));
    memset(&value, 0, sizeof(value));
    sixel_runtime_policy_load(&options);
    if (options.resize_precision_override) {
        return options.resize_precision;
    }
    if (sixel_option_registry_resolve_runtime_binding(
            SIXEL_SUBOPTION_BINDING_ID_2(
                resize_precision,
                resize_precision_override),
            SIXEL_SUBOPTION_VALUE_CHOICE,
            &value) == SIXEL_OPTION_ENVIRONMENT_MATCH) {
        return value.int_value;
    }
    return fallback;
}

size_t
sixel_runtime_policy_scale_min_bytes(size_t fallback)
{
    sixel_runtime_policy_options_t options;
    sixel_suboption_value_t value;

    memset(&options, 0, sizeof(options));
    memset(&value, 0, sizeof(value));
    sixel_runtime_policy_load(&options);
    if (options.scale_parallel_min_bytes_override) {
        return options.scale_parallel_min_bytes;
    }
    if (sixel_option_registry_resolve_runtime_binding(
            SIXEL_SUBOPTION_BINDING_ID_2(
                scale_parallel_min_bytes,
                scale_parallel_min_bytes_override),
            SIXEL_SUBOPTION_VALUE_SIZE,
            &value) == SIXEL_OPTION_ENVIRONMENT_MATCH) {
        return value.size_value;
    }
    return fallback;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
