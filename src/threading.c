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

/* OpenVMS needs X/Open extensions for time types. Limit the selection to
 * time.h: exposing network extensions under strict POSIX hides BSD types
 * required by those declarations. Preserve any caller-selected namespace. */
#if defined(__VMS) && !defined(_XOPEN_SOURCE_EXTENDED)
# define _XOPEN_SOURCE_EXTENDED 1
# include <time.h>
# undef _XOPEN_SOURCE_EXTENDED
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
#include <time.h>

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
#include "pthread-once.h"
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

/* Keep native identity operations in the same portability owner as locks.
 * A serialized build has one logical caller and needs no pthread symbols. */
sixel_thread_identity_t
sixel_thread_identity_current(void)
{
#if SIXEL_USE_WIN32_THREADS
    return GetCurrentThreadId();
#elif SIXEL_USE_PTHREADS
    return pthread_self();
#else
    sixel_thread_identity_t identity;

    memset(&identity, 0, sizeof(identity));
    return identity;
#endif
}

int
sixel_thread_identity_equal(sixel_thread_identity_t left,
                             sixel_thread_identity_t right)
{
#if SIXEL_USE_WIN32_THREADS
    return left == right;
#elif SIXEL_USE_PTHREADS
    return pthread_equal(left, right) != 0;
#else
    (void)left;
    (void)right;
    return 1;
#endif
}

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

SIXELAPI int
sixel_cond_timedwait(sixel_cond_t *cond, sixel_mutex_t *mutex, int timeout_ms)
{
    struct timespec deadline;
    struct timeval now;
    long nanoseconds;
    int rc;

    if (cond == NULL || mutex == NULL) {
        return 1;
    }
    if (timeout_ms < 0) {
        timeout_ms = 0;
    }
    /* Default pthread conditions use absolute wall-clock deadlines. The
     * compatibility helper also supports libc without clock_gettime(). */
    if (sixel_compat_gettimeofday(&now) != 0) {
        return 1;
    }
    deadline.tv_sec = now.tv_sec + timeout_ms / 1000;
    nanoseconds = now.tv_usec * 1000L +
        (long)(timeout_ms % 1000) * 1000000L;
    deadline.tv_sec += nanoseconds / 1000000000L;
    deadline.tv_nsec = nanoseconds % 1000000000L;
    rc = pthread_cond_timedwait(&cond->native, &mutex->native, &deadline);
#if HAVE_ERRNO_H
    if (rc == ETIMEDOUT) {
        return 1;
    }
    if (rc == EINTR) {
        return 0;
    }
#endif
    if (rc != 0) {
        sixel_pthread_abort("pthread_cond_timedwait", rc);
    }
    return 0;
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

SIXELAPI int
sixel_cond_timedwait(sixel_cond_t *cond, sixel_mutex_t *mutex, int timeout_ms)
{
    BOOL rc;
    DWORD error;

    if (cond == NULL || mutex == NULL) {
        return 1;
    }
    if (timeout_ms < 0) {
        timeout_ms = 0;
    }
    rc = SleepConditionVariableCS(&cond->native, &mutex->native,
                                  (DWORD)timeout_ms);
    if (rc == 0) {
        error = GetLastError();
        if (error == ERROR_TIMEOUT) {
            return 1;
        }
        sixel_win32_abort("SleepConditionVariableCS", error);
    }
    return 0;
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

SIXELAPI int
sixel_cond_timedwait(sixel_cond_t *cond, sixel_mutex_t *mutex, int timeout_ms)
{
    (void)cond;
    (void)mutex;
    (void)timeout_ms;
    return 1;
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

/* Probe only documented hybrid-CPU interfaces. Missing or malformed
 * topology information leaves the ordinary logical-CPU budget intact. */
int
sixel_get_performance_threads(void)
{
    int logical;
#if defined(__APPLE__) && HAVE_SYS_SYSCTL_H
    int levels;
    int performance;
    size_t size;
#elif defined(__linux__)
    FILE *stream;
    char cpus[4096];
    char *cursor;
    char *end;
    long first;
    long last;
    long previous;
    long count;
#endif

    logical = sixel_get_hw_threads();
#if defined(__APPLE__) && HAVE_SYS_SYSCTL_H
    levels = 0;
    performance = 0;
    size = sizeof(levels);
    if (sysctlbyname("hw.nperflevels", &levels, &size, NULL, 0) == 0 &&
            levels > 1) {
        size = sizeof(performance);
        if (sysctlbyname("hw.perflevel0.logicalcpu", &performance,
                        &size, NULL, 0) == 0 &&
                performance > 0 && performance < logical) {
            return performance;
        }
    }
#elif defined(__linux__)
    stream = fopen("/sys/devices/cpu_core/cpus", "r");
    if (stream == NULL) {
        return logical;
    }
    /* Require the complete line, so a truncated list cannot undercount. */
    cursor = fgets(cpus, sizeof(cpus), stream);
    if (cursor == NULL || fgetc(stream) != EOF || ferror(stream)) {
        fclose(stream);
        return logical;
    }
    fclose(stream);
    count = 0;
    previous = -1;
    for (;;) {
        if (*cursor < '0' || *cursor > '9') {
            return logical;
        }
        errno = 0;
        first = strtol(cursor, &end, 10);
        if (errno == ERANGE || first > INT_MAX || first <= previous) {
            return logical;
        }
        last = first;
        cursor = end;
        if (*cursor == '-') {
            ++cursor;
            if (*cursor < '0' || *cursor > '9') {
                return logical;
            }
            errno = 0;
            last = strtol(cursor, &end, 10);
            if (errno == ERANGE || last > INT_MAX || last < first) {
                return logical;
            }
            cursor = end;
        }
        if (last - first >= INT_MAX - count) {
            return logical;
        }
        count += last - first + 1;
        previous = last;
        if (*cursor != ',') {
            break;
        }
        ++cursor;
    }
    if (*cursor == '\n') {
        ++cursor;
    }
    if (*cursor == '\0' && count > 0 && count < logical) {
        return (int)count;
    }
#endif
    return logical > 0 ? logical : 1;
}

#define SIXEL_PROCESSING_THREADS_DEFAULT 4

/* Only low-level budgets are process defaults. Object policy never writes
 * this store; both sides are protected by the configuration mutex. */
static unsigned int g_encoder_budget;
static int g_encoder_budget_set;
static unsigned int g_decoder_budget;
static int g_decoder_budget_set;

static sixel_runtime_policy_options_t g_runtime_policy;
static sixel_diagnostics_policy_options_t g_diagnostics_policy;
static sixel_clipboard_policy_options_t g_clipboard_policy;

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
SIXEL_PTHREAD_ONCE_DECLARE(g_thread_config_once);

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

static sixel_thread_budget_scope_t *g_thread_budget_scopes;

void
sixel_threading_policy_enter(sixel_thread_budget_scope_t *scope,
                              sixel_threading_snapshot_t const *snapshot)
{
    scope->owner = sixel_thread_identity_current();
    scope->policy = *snapshot;
    scope->policy_root = 1;
    scope->threads = snapshot->budget;
    scope->ceiling = scope->threads;
    scope->reclaim = NULL;
    scope->reclaim_context = NULL;
    sixel_thread_config_lock();
    scope->next = g_thread_budget_scopes;
    g_thread_budget_scopes = scope;
    sixel_thread_config_unlock();
}

void
sixel_threading_object_enter(sixel_thread_budget_scope_t *scope,
                              sixel_threading_policy_options_t const *options,
                              int decoder)
{
    sixel_threading_snapshot_t snapshot;

    sixel_threading_policy_resolve(options, decoder, &snapshot);
    sixel_threading_policy_enter(scope, &snapshot);
}

void
sixel_thread_budget_enter(sixel_thread_budget_scope_t *scope, int threads)
{
    sixel_threading_snapshot_t snapshot;

    (void)sixel_threading_policy_capture(&snapshot, 0);
    sixel_threading_policy_enter(scope, &snapshot);
    sixel_thread_config_lock();
    scope->policy_root = 0;
    scope->threads = threads > 0 ? threads : 1;
    scope->ceiling = scope->threads;
    sixel_thread_config_unlock();
}

void
sixel_thread_budget_leave(sixel_thread_budget_scope_t *scope)
{
    sixel_thread_budget_scope_t **link;

    sixel_thread_config_lock();
    for (link = &g_thread_budget_scopes; *link != NULL;
            link = &(*link)->next) {
        if (*link == scope) {
            *link = scope->next;
            break;
        }
    }
    sixel_thread_config_unlock();
}

static int
sixel_threads_resolve_limit(int ceiling)
{
    int resolved;
    int limit;
    sixel_thread_budget_scope_t *scope;
    sixel_thread_identity_t owner;

    sixel_threading_snapshot_t snapshot;

    (void)sixel_threading_policy_capture(&snapshot, 0);
    resolved = snapshot.budget;
    owner = sixel_thread_identity_current();
    sixel_thread_config_lock();
    for (scope = g_thread_budget_scopes; scope != NULL;
            scope = scope->next) {
        limit = ceiling ? scope->ceiling : scope->threads;
        if (sixel_thread_identity_equal(scope->owner, owner)) {
            if (limit < resolved) {
                resolved = limit;
            }
            if (scope->policy_root) {
                break;
            }
        }
    }
    sixel_thread_config_unlock();

    return resolved;
}

SIXELAPI int
sixel_threads_resolve(void)
{
    return sixel_threads_resolve_limit(0);
}

int
sixel_threads_ceiling(void)
{
    return sixel_threads_resolve_limit(1);
}

void
sixel_thread_budget_set_reclaim(sixel_thread_budget_scope_t *scope,
                               int ceiling, int (*reclaim)(void *),
                               void *context)
{
    sixel_thread_config_lock();
    scope->ceiling = ceiling > scope->threads ? ceiling : scope->threads;
    scope->reclaim = reclaim;
    scope->reclaim_context = context;
    sixel_thread_config_unlock();
}

int
sixel_threads_reclaim(void)
{
    sixel_thread_budget_scope_t *scope;
    sixel_thread_identity_t owner;

    owner = sixel_thread_identity_current();
    for (;;) {
        sixel_thread_config_lock();
        for (scope = g_thread_budget_scopes; scope != NULL;
                scope = scope->next) {
            if (sixel_thread_identity_equal(scope->owner, owner)) {
                if (scope->reclaim != NULL) {
                    break;
                }
                if (scope->policy_root) {
                    scope = NULL;
                    break;
                }
            }
        }
        sixel_thread_config_unlock();
        /* Only the owner can leave its scope, so it stays alive while its
         * callback polls and joins the donor outside the global lock. */
        if (scope == NULL || !scope->reclaim(scope->reclaim_context)) {
            break;
        }
        sixel_thread_config_lock();
        scope->threads = scope->ceiling;
        scope->reclaim = NULL;
        sixel_thread_config_unlock();
    }
    return sixel_threads_resolve();
}

/* Set these defaults before processing; in-flight snapshots stay fixed. */
SIXELAPI void
sixel_set_encoder_threads(int budget)
{
    sixel_thread_config_lock();
    g_encoder_budget = budget > 0 ? (unsigned int)budget : 0u;
    g_encoder_budget_set = 1;
    sixel_thread_config_unlock();
}

SIXELAPI void
sixel_set_decoder_threads(int budget)
{
    sixel_thread_config_lock();
    g_decoder_budget = budget > 0 ? (unsigned int)budget : 0u;
    g_decoder_budget_set = 1;
    sixel_thread_config_unlock();
}

void
sixel_threading_policy_resolve(sixel_threading_policy_options_t const *input,
                               int decoder,
                               sixel_threading_snapshot_t *snapshot)
{
    sixel_option_argument_schema_t const *schema;
    sixel_option_schema_id_t id;
    sixel_threading_policy_options_t *options;
    unsigned int consumer;
    unsigned int api_budget;
    int api_set;
    int performance;

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->decoder = decoder;
    options = &snapshot->options;
    consumer = decoder ? SIXEL_OPTION_SCOPE_DECODER
                       : SIXEL_OPTION_SCOPE_ENCODER;
    id = decoder ? SIXEL_OPTION_SCHEMA_DECODER_THREADING_POLICY
                 : SIXEL_OPTION_SCHEMA_ENCODER_THREADING_POLICY;
    schema = sixel_option_registry_get(id);
    options->mode = SIXEL_THREADING_POLICY_BALANCED;
    options->mode_override = sixel_option_resolve_registered_base_environment(
        id, consumer, &options->mode);
    options->load = SIXEL_THREADING_FRAME_AHEAD;
    options->prepare = SIXEL_THREADING_FRAME_AHEAD;
    options->load_depth = SIXEL_THREADING_LOAD_DEPTH_DEFAULT;
    sixel_option_apply_suboption_environment(schema, NULL, consumer, options,
                                             SIXEL_SUBOPTION_TARGET_THREADING);
    sixel_thread_config_lock();
    api_budget = decoder ? g_decoder_budget : g_encoder_budget;
    api_set = decoder ? g_decoder_budget_set : g_encoder_budget_set;
    sixel_thread_config_unlock();
    if (api_set) {
        options->budget = api_budget;
        options->budget_override = 1;
    }
    /* Merge only explicit object fields, above the API and environment.
     * Keeping override bits allows automatic dither/encode allocation and
     * the distinct default pin choices of dither and FHEDT consumers. */
#define SIXEL_COPY_THREADING_FIELD(field) \
    if (input != NULL && input->field##_override) { \
        options->field = input->field; \
        options->field##_override = 1; \
    }
    SIXEL_COPY_THREADING_FIELD(mode)
    SIXEL_COPY_THREADING_FIELD(budget)
    if (!decoder) {
        SIXEL_COPY_THREADING_FIELD(load)
        SIXEL_COPY_THREADING_FIELD(load_depth)
        SIXEL_COPY_THREADING_FIELD(prepare)
        SIXEL_COPY_THREADING_FIELD(palette)
        SIXEL_COPY_THREADING_FIELD(dither_threads)
        SIXEL_COPY_THREADING_FIELD(encode_threads)
        SIXEL_COPY_THREADING_FIELD(prepare_threads)
        SIXEL_COPY_THREADING_FIELD(decode_threads)
        SIXEL_COPY_THREADING_FIELD(pin)
    }
#undef SIXEL_COPY_THREADING_FIELD
    performance = sixel_get_performance_threads();
    snapshot->explicit_budget = options->budget_override
                                || options->mode_override;
    if (options->budget_override) {
        snapshot->budget = sixel_threads_normalize((int)options->budget);
    } else if (decoder && !snapshot->explicit_budget) {
        snapshot->budget = 1;
    } else {
        snapshot->budget = performance < SIXEL_PROCESSING_THREADS_DEFAULT
            ? performance : SIXEL_PROCESSING_THREADS_DEFAULT;
        snapshot->budget = sixel_threads_normalize(snapshot->budget);
    }
    if (options->mode == SIXEL_THREADING_POLICY_BALANCED &&
            snapshot->budget > performance) {
        snapshot->budget = performance;
    }
}

int
sixel_threading_policy_capture(sixel_threading_snapshot_t *snapshot,
                                int decoder)
{
    sixel_thread_budget_scope_t *scope;
    sixel_thread_identity_t owner;
    int found;

    found = 0;
    owner = sixel_thread_identity_current();
    sixel_thread_config_lock();
    for (scope = g_thread_budget_scopes; scope != NULL; scope = scope->next) {
        if (sixel_thread_identity_equal(scope->owner, owner)) {
            *snapshot = scope->policy;
            found = 1;
            break;
        }
    }
    sixel_thread_config_unlock();
    if (!found) {
        sixel_threading_policy_resolve(NULL, decoder, snapshot);
    }
    return found;
}

void
sixel_threading_policy_load(sixel_threading_policy_options_t *options)
{
    sixel_threading_snapshot_t snapshot;

    (void)sixel_threading_policy_capture(&snapshot, 0);
    *options = snapshot.options;
}

int
sixel_threading_policy_mode(void)
{
    sixel_threading_policy_options_t options;

    sixel_threading_policy_load(&options);
    return options.mode;
}

char const *
sixel_threading_policy_mode_name(void)
{
    switch (sixel_threading_policy_mode()) {
    case SIXEL_THREADING_POLICY_LAZY:
        return "lazy";
    case SIXEL_THREADING_POLICY_GREEDY:
        return "greedy";
    default:
        return "balanced";
    }
}

int
sixel_threads_resolve_fanout(void)
{
    if (sixel_threading_policy_mode() == SIXEL_THREADING_POLICY_LAZY) {
        return 1;
    }
    return sixel_threads_resolve();
}

int
sixel_threading_policy_explicit_budget(int *threads)
{
    sixel_threading_snapshot_t snapshot;

    (void)sixel_threading_policy_capture(&snapshot, 0);
    if (threads != NULL) {
        *threads = snapshot.budget;
    }
    return snapshot.explicit_budget;
}

int
sixel_threading_policy_load_mode(void)
{
    sixel_threading_policy_options_t options;

    sixel_threading_policy_load(&options);
    return options.load;
}

int
sixel_threading_policy_prepare_mode(void)
{
    sixel_threading_policy_options_t options;

    sixel_threading_policy_load(&options);
    return options.prepare;
}

int
sixel_threading_policy_palette_mode(void)
{
    sixel_threading_policy_options_t options;

    sixel_threading_policy_load(&options);
    return options.palette;
}

int
sixel_threading_policy_load_depth(void)
{
    sixel_threading_policy_options_t options;

    sixel_threading_policy_load(&options);
    return (int)options.load_depth;
}

int
sixel_threading_policy_stage_threads(sixel_threading_stage_t stage,
                                     unsigned int *count)
{
    sixel_threading_policy_options_t options;

    if (count == NULL) {
        return 0;
    }
    sixel_threading_policy_load(&options);
    switch (stage) {
    case SIXEL_THREADING_STAGE_DITHER:
        *count = options.dither_threads;
        return options.dither_threads_override;
    case SIXEL_THREADING_STAGE_ENCODE:
        *count = options.encode_threads;
        return options.encode_threads_override;
    case SIXEL_THREADING_STAGE_PREPARE:
        *count = options.prepare_threads;
        return options.prepare_threads_override;
    case SIXEL_THREADING_STAGE_DECODE:
        *count = options.decode_threads;
        return options.decode_threads_override;
    default:
        return 0;
    }
}

int
sixel_threading_policy_pin(int fallback)
{
    sixel_threading_policy_options_t options;

    sixel_threading_policy_load(&options);
    return options.pin_override ? options.pin != 0 : fallback;
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
sixel_process_policy_duplicate_string(char const *text)
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
        trace_topic_copy = sixel_process_policy_duplicate_string(
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

void
sixel_clipboard_policy_load(sixel_clipboard_policy_options_t *options)
{
    if (options == NULL) {
        return;
    }
    sixel_thread_config_lock();
    *options = g_clipboard_policy;
    options->directory = NULL;
    sixel_thread_config_unlock();
}

int
sixel_clipboard_policy_store(
    sixel_clipboard_policy_options_t const *options)
{
    char *directory_copy;
    char *old_directory;

    directory_copy = NULL;
    old_directory = NULL;
    if (options == NULL) {
        return 0;
    }
    if (options->directory_override) {
        directory_copy = sixel_process_policy_duplicate_string(
            options->directory);
        if (directory_copy == NULL) {
            return 0;
        }
    }
    sixel_thread_config_lock();
    old_directory = (char *)g_clipboard_policy.directory;
    g_clipboard_policy = *options;
    g_clipboard_policy.directory = directory_copy;
    sixel_thread_config_unlock();
    free(old_directory);
    return 1;
}

int
sixel_clipboard_policy_copy_directory_override(char *buffer,
                                                size_t buffer_size,
                                                int *configured)
{
    size_t length;

    length = 0u;
    if (buffer == NULL || buffer_size == 0u || configured == NULL) {
        return 0;
    }
    buffer[0] = '\0';
    *configured = 0;
    sixel_thread_config_lock();
    *configured = g_clipboard_policy.directory_override != 0;
    if (!*configured) {
        sixel_thread_config_unlock();
        return 1;
    }
    if (g_clipboard_policy.directory == NULL) {
        sixel_thread_config_unlock();
        return 0;
    }
    length = strlen(g_clipboard_policy.directory) + 1u;
    if (length > buffer_size) {
        sixel_thread_config_unlock();
        return 0;
    }
    memcpy(buffer, g_clipboard_policy.directory, length);
    sixel_thread_config_unlock();
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

    /* Greedy overrides even explicitly configured runtime thresholds. */
    if (sixel_threading_policy_mode() == SIXEL_THREADING_POLICY_GREEDY) {
        return 0u;
    }
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

size_t
sixel_runtime_policy_sample_min_pixels(size_t fallback)
{
    sixel_runtime_policy_options_t options;
    sixel_suboption_value_t value;

    /* Greedy overrides even explicitly configured runtime thresholds. */
    if (sixel_threading_policy_mode() == SIXEL_THREADING_POLICY_GREEDY) {
        return 0u;
    }
    memset(&options, 0, sizeof(options));
    memset(&value, 0, sizeof(value));
    sixel_runtime_policy_load(&options);
    if (options.sample_parallel_min_pixels_override) {
        return options.sample_parallel_min_pixels;
    }
    if (sixel_option_registry_resolve_runtime_binding(
            SIXEL_SUBOPTION_BINDING_ID_2(
                sample_parallel_min_pixels,
                sample_parallel_min_pixels_override),
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

    /* Greedy overrides even explicitly configured runtime thresholds. */
    if (sixel_threading_policy_mode() == SIXEL_THREADING_POLICY_GREEDY) {
        return 0u;
    }
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
