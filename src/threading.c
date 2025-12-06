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

#include "config.h"

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
#if HAVE_SYS_SYSCTL_H
# include <sys/sysctl.h>
#endif

#include "threading.h"

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
    rc = pthread_cond_wait(&cond->native, &mutex->native);
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

SIXELAPI int
sixel_get_hw_threads(void)
{
    long count;
#if defined(_SC_NPROCESSORS_ONLN)
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
sixel_get_hw_threads(void)
{
    return 1;
}

#endif /* SIXEL_USE_PTHREADS */

/*
 * Thread configuration keeps the precedence rules centralized:
 *   1. Library callers may override via `sixel_set_threads`.
 *   2. Otherwise, `SIXEL_THREADS` from the environment is honored.
 *   3. Fallback defaults to single threaded execution.
 */
typedef struct sixel_thread_config_state {
    int requested_threads;
    int override_active;
    int env_threads;
    int env_valid;
    int env_checked;
} sixel_thread_config_state_t;

static sixel_thread_config_state_t g_thread_config = {
    1,
    0,
    1,
    0,
    0
};

static int
sixel_threads_token_is_auto(char const *text)
{
    if (text == NULL) {
        return 0;
    }

    if ((text[0] == 'a' || text[0] == 'A') &&
        (text[1] == 'u' || text[1] == 'U') &&
        (text[2] == 't' || text[2] == 'T') &&
        (text[3] == 'o' || text[3] == 'O') &&
        text[4] == '\0') {
        return 1;
    }

    return 0;
}

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

static int
sixel_threads_parse_env_value(char const *text, int *value)
{
    long parsed;
    char *endptr;
    int normalized;

    if (text == NULL || value == NULL) {
        return 0;
    }

    if (sixel_threads_token_is_auto(text)) {
        normalized = sixel_threads_normalize(0);
        *value = normalized;
        return 1;
    }

    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0' || errno == ERANGE) {
        return 0;
    }

    if (parsed < 1) {
        normalized = sixel_threads_normalize(1);
    } else if (parsed > INT_MAX) {
        normalized = sixel_threads_normalize(INT_MAX);
    } else {
        normalized = sixel_threads_normalize((int)parsed);
    }

    *value = normalized;
    return 1;
}

static void
sixel_threads_load_env(void)
{
    char const *text;
    int parsed;

    if (g_thread_config.env_checked) {
        return;
    }

    g_thread_config.env_checked = 1;
    g_thread_config.env_valid = 0;

    text = getenv("SIXEL_THREADS");
    if (text == NULL || text[0] == '\0') {
        return;
    }

    if (sixel_threads_parse_env_value(text, &parsed)) {
        g_thread_config.env_threads = parsed;
        g_thread_config.env_valid = 1;
    }
}

SIXELAPI int
sixel_threads_resolve(void)
{
    int resolved;

#if SIXEL_ENABLE_THREADS
    if (g_thread_config.override_active) {
        return g_thread_config.requested_threads;
    }
#endif

    sixel_threads_load_env();

#if SIXEL_ENABLE_THREADS
    if (g_thread_config.env_valid) {
        resolved = g_thread_config.env_threads;
    } else {
        resolved = sixel_threads_normalize(0);
    }
#else
    resolved = 1;
#endif

    return resolved;
}

/*
 * Public setter so CLI/bindings may override the runtime thread preference.
 */
SIXELAPI void
sixel_set_threads(int threads)
{
#if SIXEL_ENABLE_THREADS
    g_thread_config.requested_threads = sixel_threads_normalize(threads);
#else
    (void)threads;
    g_thread_config.requested_threads = 1;
#endif
    g_thread_config.override_active = 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
