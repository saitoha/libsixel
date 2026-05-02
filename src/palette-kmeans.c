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

/*
 * This translation unit owns the K-means palette quantizer.  The structure is
 * organised so palette.c can delegate the algorithm-specific work while it
 * continues handling configuration and result publication.  The processing
 * pipeline follows the stages below:
 *
 *   [sample collection] -> [k-means++ seeding] -> [Lloyd iteration]
 *                      -> [optional final merge] -> [palette export]
 *
 * Each stage is implemented in a dedicated block with extensive comments so
 * future maintainers can reason about the data flow without cross-referencing
 * the orchestrator.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_FLOAT_H
# include <float.h>
#endif

#include "allocator.h"
#include "compat_stub.h"
#include "timeline-logger.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-kmeans.h"
#include "palette-kmeans-build.h"
#include "palette.h"
#include "pixelformat.h"
#include "status.h"
#include "timer.h"


#if defined(_MSC_VER)
# define SIXEL_TLS __declspec(thread)
# define SIXEL_KMEANS_TLS_AVAILABLE 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !defined(__PCC__)
# define SIXEL_TLS _Thread_local
# define SIXEL_KMEANS_TLS_AVAILABLE 1
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__PCC__)
# define SIXEL_TLS __thread
# define SIXEL_KMEANS_TLS_AVAILABLE 1
#else
# define SIXEL_TLS
# define SIXEL_KMEANS_TLS_AVAILABLE 0
#endif

static SIXEL_TLS int sixel_kmeans_init_type_override_enabled = 0;
static SIXEL_TLS sixel_kmeans_init_type sixel_kmeans_init_type_override_value
    = SIXEL_PALETTE_KMEANS_INIT_AUTO;
static SIXEL_TLS int sixel_kmeans_binning_mode_override_enabled = 0;
static SIXEL_TLS sixel_kmeans_binning_mode
    sixel_kmeans_binning_mode_override_value
        = SIXEL_PALETTE_KMEANS_BINNING_AUTO;
static SIXEL_TLS int sixel_kmeans_binbits_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmeans_binbits_override_value = 6u;
static SIXEL_TLS int sixel_kmeans_mapping_mode_override_enabled = 0;
static SIXEL_TLS sixel_kmeans_mapping_mode
    sixel_kmeans_mapping_mode_override_value
        = SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM;
static SIXEL_TLS int sixel_kmeans_softdist_mode_override_enabled = 0;
static SIXEL_TLS sixel_kmeans_softdist_mode
    sixel_kmeans_softdist_mode_override_value
        = SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR;
static SIXEL_TLS int sixel_kmeans_autoratio_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmeans_autoratio_override_value = 32u;
static SIXEL_TLS int sixel_kmeans_seed_override_enabled = 0;
static SIXEL_TLS uint32_t sixel_kmeans_seed_override_value = 0u;
static SIXEL_TLS int sixel_kmeans_restarts_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmeans_restarts_override_value = 1u;
static SIXEL_TLS int sixel_kmeans_iter_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmeans_iter_override_value = 0u;
static SIXEL_TLS int sixel_kmeans_miniter_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmeans_miniter_override_value = 0u;
static SIXEL_TLS int sixel_kmeans_polish_iter_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmeans_polish_iter_override_value = 0u;
static SIXEL_TLS int sixel_kmeans_feedback_mode_override_enabled = 0;
static SIXEL_TLS sixel_kmeans_feedback_mode
    sixel_kmeans_feedback_mode_override_value
        = SIXEL_PALETTE_KMEANS_FEEDBACK_OFF;
static SIXEL_TLS int sixel_kmeans_prune_policy_override_enabled = 0;
static SIXEL_TLS sixel_kmeans_prune_policy
    sixel_kmeans_prune_policy_override_value
        = SIXEL_PALETTE_KMEANS_PRUNE_AUTO;
static SIXEL_TLS int sixel_kmeans_feedback_slots_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmeans_feedback_slots_override_value = 1u;
static SIXEL_TLS int sixel_kmeans_feedback_interval_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmeans_feedback_interval_override_value
    = 1u;

#undef SIXEL_TLS

#if SIXEL_ENABLE_THREADS && !SIXEL_KMEANS_TLS_AVAILABLE
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
#  if !defined(UNICODE)
#   define UNICODE
#  endif
#  if !defined(_UNICODE)
#   define _UNICODE
#  endif
#  if !defined(WIN32_LEAN_AND_MEAN)
#   define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
static CRITICAL_SECTION sixel_kmeans_override_mutex;
static INIT_ONCE sixel_kmeans_override_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
sixel_kmeans_override_lock_init_once(PINIT_ONCE once,
                                     PVOID parameter,
                                     PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;

    InitializeCriticalSection(&sixel_kmeans_override_mutex);
    return TRUE;
}
# else
#  include <pthread.h>
/*
 * Use runtime initialization so Cosmopolitan/pcc builds avoid warnings from
 * partial pthread struct initializers.
 */
static pthread_mutex_t sixel_kmeans_override_mutex;
static pthread_once_t sixel_kmeans_override_mutex_once = PTHREAD_ONCE_INIT;
static int sixel_kmeans_override_mutex_ready = 0;

static void
sixel_kmeans_override_lock_init_once(void)
{
    int rc;

    rc = pthread_mutex_init(&sixel_kmeans_override_mutex, NULL);
    if (rc == 0) {
        sixel_kmeans_override_mutex_ready = 1;
    }
}
# endif
#endif

/*
 * K-means tunables are normally thread-local.  When TLS is unavailable they
 * collapse into process globals, so protect override access in threaded builds.
 */
int
sixel_kmeans_override_lock_acquire(void)
{
#if SIXEL_ENABLE_THREADS && !SIXEL_KMEANS_TLS_AVAILABLE
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    BOOL initialized;

    initialized = InitOnceExecuteOnce(&sixel_kmeans_override_once,
                                      sixel_kmeans_override_lock_init_once,
                                      NULL,
                                      NULL);
    if (!initialized) {
        return 0;
    }
    EnterCriticalSection(&sixel_kmeans_override_mutex);
    return 1;
# else
    int rc;
    int once_status;

    once_status = pthread_once(&sixel_kmeans_override_mutex_once,
                               sixel_kmeans_override_lock_init_once);
    if (once_status != 0 || !sixel_kmeans_override_mutex_ready) {
        return 0;
    }

    rc = pthread_mutex_lock(&sixel_kmeans_override_mutex);
    if (rc != 0) {
        return 0;
    }
    return 1;
# endif
#else
    return 0;
#endif
}

void
sixel_kmeans_override_lock_release(int acquired)
{
    if (acquired == 0) {
        return;
    }
#if SIXEL_ENABLE_THREADS && !SIXEL_KMEANS_TLS_AVAILABLE
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    LeaveCriticalSection(&sixel_kmeans_override_mutex);
# else
    if (!sixel_kmeans_override_mutex_ready) {
        return;
    }
    (void)pthread_mutex_unlock(&sixel_kmeans_override_mutex);
# endif
#endif
}

#undef SIXEL_KMEANS_TLS_AVAILABLE

typedef struct sixel_kmeans_projection_entry {
    double projection;
    double weight;
    unsigned int index;
} sixel_kmeans_projection_entry_t;

int
sixel_palette_kmeans_log_start(sixel_timeline_logger_t *logger,
                               int *job_seq,
                               char const *engine_name,
                               char const *role,
                               char const *phase)
{
    int job_id;

    job_id = -1;
    if (logger == NULL) {
        return job_id;
    }
    if (job_seq != NULL) {
        job_id = *job_seq;
        *job_seq += 1;
    }
    sixel_timeline_logger_logf(logger,
                      (role != NULL && role[0] != '\0') ? role : "palette",
                      "palette/build",
                      "start",
                      job_id,
                      -1,
                      0,
                      0,
                      0,
                      0,
                      "engine=%s phase=%s",
                      engine_name != NULL ? engine_name : "(unknown)",
                      phase);
    return job_id;
}

void
sixel_palette_kmeans_log_finish(sixel_timeline_logger_t *logger,
                                int job_id,
                                char const *engine_name,
                                char const *role,
                                char const *phase,
                                char const *detail)
{
    char const *suffix;

    if (logger == NULL || job_id < 0) {
        return;
    }
    suffix = "";
    if (detail != NULL && detail[0] != '\0') {
        suffix = detail;
    }
    sixel_timeline_logger_logf(logger,
                      (role != NULL && role[0] != '\0') ? role : "palette",
                      "palette/build",
                      "finish",
                      job_id,
                      -1,
                      0,
                      0,
                      0,
                      0,
                      "engine=%s phase=%s%s%s",
                      engine_name != NULL ? engine_name : "(unknown)",
                      phase,
                      suffix[0] != '\0' ? " " : "",
                      suffix);
}

static const char *
sixel_kmeans_init_type_to_string(sixel_kmeans_init_type init_type)
{
    switch (init_type) {
    case SIXEL_PALETTE_KMEANS_INIT_NONE:
        return "none";
    case SIXEL_PALETTE_KMEANS_INIT_PCA:
        return "pca";
    case SIXEL_PALETTE_KMEANS_INIT_AUTO:
    default:
        return "auto";
    }
}

static sixel_kmeans_init_type
sixel_kmeans_resolve_init_type(sixel_kmeans_init_type init_type)
{
    if (init_type == SIXEL_PALETTE_KMEANS_INIT_PCA) {
        return init_type;
    }

    return SIXEL_PALETTE_KMEANS_INIT_NONE;
}

void
sixel_set_kmeans_init_type_override(int enabled,
                                    sixel_kmeans_init_type init_type)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_init_type_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_init_type_override_value = init_type;
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXELAPI sixel_kmeans_init_type
sixel_get_kmeans_init_type(void)
{
    sixel_kmeans_init_type parsed;
    sixel_kmeans_init_type resolved;
    char const *env_value;
    static int init_loaded = 0;
    static sixel_kmeans_init_type cached_value
        = SIXEL_PALETTE_KMEANS_INIT_PCA;

    parsed = SIXEL_PALETTE_KMEANS_INIT_AUTO;
    resolved = SIXEL_PALETTE_KMEANS_INIT_PCA;
    env_value = NULL;
    if (sixel_kmeans_init_type_override_enabled) {
        return sixel_kmeans_resolve_init_type(
            sixel_kmeans_init_type_override_value);
    }
    if (init_loaded) {
        return cached_value;
    }
    init_loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_INITTYPE");
    if (env_value != NULL && env_value[0] != '\0') {
        if (sixel_compat_strcasecmp(env_value, "none") == 0) {
            parsed = SIXEL_PALETTE_KMEANS_INIT_NONE;
        } else if (sixel_compat_strcasecmp(env_value, "pca") == 0) {
            parsed = SIXEL_PALETTE_KMEANS_INIT_PCA;
        } else if (sixel_compat_strcasecmp(env_value, "auto") == 0) {
            parsed = SIXEL_PALETTE_KMEANS_INIT_AUTO;
        }
    }

    resolved = sixel_kmeans_resolve_init_type(parsed);
    cached_value = resolved;
    sixel_debugf("k-means init type: %s",
                 sixel_kmeans_init_type_to_string(resolved));

    return resolved;
}

static sixel_kmeans_binning_mode
sixel_kmeans_resolve_binning_mode(sixel_kmeans_binning_mode mode)
{
    switch (mode) {
    case SIXEL_PALETTE_KMEANS_BINNING_NONE:
    case SIXEL_PALETTE_KMEANS_BINNING_HARD:
    case SIXEL_PALETTE_KMEANS_BINNING_SOFT:
    case SIXEL_PALETTE_KMEANS_BINNING_AUTO:
        return mode;
    default:
        return SIXEL_PALETTE_KMEANS_BINNING_AUTO;
    }
}

void
sixel_set_kmeans_binning_mode_override(int enabled,
                                       sixel_kmeans_binning_mode mode)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_binning_mode_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_binning_mode_override_value
        = sixel_kmeans_resolve_binning_mode(mode);
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kmeans_binning_mode
sixel_get_kmeans_binning_mode(void)
{
    char const *env_value;
    static int loaded = 0;
    static sixel_kmeans_binning_mode cached
        = SIXEL_PALETTE_KMEANS_BINNING_AUTO;

    env_value = NULL;
    if (sixel_kmeans_binning_mode_override_enabled) {
        return sixel_kmeans_binning_mode_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_BINNING");
    if (env_value != NULL && env_value[0] != '\0') {
        if (sixel_compat_strcasecmp(env_value, "none") == 0) {
            cached = SIXEL_PALETTE_KMEANS_BINNING_NONE;
        } else if (sixel_compat_strcasecmp(env_value, "hard") == 0) {
            cached = SIXEL_PALETTE_KMEANS_BINNING_HARD;
        } else if (sixel_compat_strcasecmp(env_value, "soft") == 0) {
            cached = SIXEL_PALETTE_KMEANS_BINNING_SOFT;
        } else if (sixel_compat_strcasecmp(env_value, "auto") == 0) {
            cached = SIXEL_PALETTE_KMEANS_BINNING_AUTO;
        }
    }

    return cached;
}

void
sixel_set_kmeans_binbits_override(int enabled,
                                  unsigned int bits)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_binbits_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_binbits_override_value = bits;
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_binbits(void)
{
    char const *env_value;
    char *endptr;
    long parsed;
    static int loaded = 0;
    static unsigned int cached = 6u;

    env_value = NULL;
    endptr = NULL;
    parsed = 0L;
    if (sixel_kmeans_binbits_override_enabled) {
        if (sixel_kmeans_binbits_override_value < 4u) {
            return 4u;
        }
        if (sixel_kmeans_binbits_override_value > 8u) {
            return 8u;
        }
        return sixel_kmeans_binbits_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_BINBITS");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed = strtol(env_value, &endptr, 10);
        if (endptr != env_value && endptr != NULL && endptr[0] == '\0'
                && errno == 0) {
            if (parsed < 4L) {
                parsed = 4L;
            }
            if (parsed > 8L) {
                parsed = 8L;
            }
            cached = (unsigned int)parsed;
        }
    }

    return cached;
}

static sixel_kmeans_mapping_mode
sixel_kmeans_resolve_mapping_mode(sixel_kmeans_mapping_mode mode)
{
    switch (mode) {
    case SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM:
    case SIXEL_PALETTE_KMEANS_MAPPING_SRGB:
        return mode;
    default:
        return SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM;
    }
}

void
sixel_set_kmeans_mapping_mode_override(int enabled,
                                       sixel_kmeans_mapping_mode mode)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_mapping_mode_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_mapping_mode_override_value
        = sixel_kmeans_resolve_mapping_mode(mode);
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kmeans_mapping_mode
sixel_get_kmeans_mapping_mode(void)
{
    char const *env_value;
    static int loaded = 0;
    static sixel_kmeans_mapping_mode cached
        = SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM;

    env_value = NULL;
    if (sixel_kmeans_mapping_mode_override_enabled) {
        return sixel_kmeans_mapping_mode_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_MAPPING");
    if (env_value != NULL && env_value[0] != '\0') {
        if (sixel_compat_strcasecmp(env_value, "srgb") == 0) {
            cached = SIXEL_PALETTE_KMEANS_MAPPING_SRGB;
        } else if (sixel_compat_strcasecmp(env_value, "uniform") == 0) {
            cached = SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM;
        }
    }

    return cached;
}

static sixel_kmeans_softdist_mode
sixel_kmeans_resolve_softdist_mode(sixel_kmeans_softdist_mode mode)
{
    switch (mode) {
    case SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR:
        return mode;
    default:
        return SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR;
    }
}

void
sixel_set_kmeans_softdist_mode_override(int enabled,
                                        sixel_kmeans_softdist_mode mode)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_softdist_mode_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_softdist_mode_override_value
        = sixel_kmeans_resolve_softdist_mode(mode);
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kmeans_softdist_mode
sixel_get_kmeans_softdist_mode(void)
{
    char const *env_value;
    static int loaded = 0;
    static sixel_kmeans_softdist_mode cached
        = SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR;

    env_value = NULL;
    if (sixel_kmeans_softdist_mode_override_enabled) {
        return sixel_kmeans_softdist_mode_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_SOFTDIST");
    if (env_value != NULL && env_value[0] != '\0') {
        if (sixel_compat_strcasecmp(env_value, "trilinear") == 0) {
            cached = SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR;
        }
    }

    return cached;
}

void
sixel_set_kmeans_autoratio_override(int enabled,
                                    unsigned int ratio)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_autoratio_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_autoratio_override_value = ratio;
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_autoratio(void)
{
    char const *env_value;
    char *endptr;
    long parsed;
    static int loaded = 0;
    static unsigned int cached = 32u;

    env_value = NULL;
    endptr = NULL;
    parsed = 0L;
    if (sixel_kmeans_autoratio_override_enabled) {
        if (sixel_kmeans_autoratio_override_value < 1u) {
            return 1u;
        }
        return sixel_kmeans_autoratio_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_AUTORATIO");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed = strtol(env_value, &endptr, 10);
        if (endptr != env_value && endptr != NULL && endptr[0] == '\0'
                && errno == 0) {
            if (parsed < 1L) {
                parsed = 1L;
            }
            if (parsed > 1048576L) {
                parsed = 1048576L;
            }
            cached = (unsigned int)parsed;
        }
    }

    return cached;
}

static unsigned int
sixel_kmeans_parse_env_uint(char const *name,
                            unsigned int minimum,
                            unsigned int maximum,
                            int allow_zero,
                            unsigned int fallback,
                            int *present_out)
{
    char const *env_value;
    char *endptr;
    unsigned long long parsed;
    unsigned int value;

    env_value = NULL;
    endptr = NULL;
    parsed = 0u;
    value = fallback;
    if (present_out != NULL) {
        *present_out = 0;
    }
    if (name == NULL) {
        return value;
    }

    env_value = sixel_compat_getenv(name);
    if (env_value == NULL || env_value[0] == '\0') {
        return value;
    }

    errno = 0;
    parsed = strtoull(env_value, &endptr, 10);
    if (endptr == env_value || endptr == NULL || endptr[0] != '\0'
            || errno != 0) {
        return value;
    }
    if (parsed > (unsigned long long)UINT_MAX) {
        parsed = (unsigned long long)UINT_MAX;
    }

    if (allow_zero && parsed == 0u) {
        if (present_out != NULL) {
            *present_out = 1;
        }
        return 0u;
    }
    if (parsed < (unsigned long long)minimum) {
        parsed = (unsigned long long)minimum;
    }
    if (parsed > (unsigned long long)maximum) {
        parsed = (unsigned long long)maximum;
    }
    if (present_out != NULL) {
        *present_out = 1;
    }

    return (unsigned int)parsed;
}

void
sixel_set_kmeans_seed_override(int enabled,
                               uint32_t seed)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_seed_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_seed_override_value = seed;
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API uint32_t
sixel_get_kmeans_seed(void)
{
    static int loaded = 0;
    static int cached_present = 0;
    static uint32_t cached = 0u;
    unsigned int parsed;
    int present;

    parsed = 0u;
    present = 0;
    if (sixel_kmeans_seed_override_enabled) {
        return sixel_kmeans_seed_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    parsed = sixel_kmeans_parse_env_uint("SIXEL_PALETTE_KMEANS_SEED",
                                         0u,
                                         0xffffffffu,
                                         1,
                                         0u,
                                         &present);
    cached = (uint32_t)parsed;
    cached_present = present;
    (void)cached_present;

    return cached;
}

SIXEL_INTERNAL_API int
sixel_get_kmeans_seed_enabled(void)
{
    static int loaded = 0;
    static int cached_present = 0;
    unsigned int parsed;

    parsed = 0u;
    if (sixel_kmeans_seed_override_enabled) {
        return 1;
    }
    if (loaded) {
        return cached_present;
    }
    loaded = 1;
    parsed = sixel_kmeans_parse_env_uint("SIXEL_PALETTE_KMEANS_SEED",
                                         0u,
                                         0xffffffffu,
                                         1,
                                         0u,
                                         &cached_present);
    (void)parsed;

    return cached_present;
}

void
sixel_set_kmeans_restarts_override(int enabled,
                                   unsigned int restarts)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_restarts_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_restarts_override_value = restarts;
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_restarts(void)
{
    static int loaded = 0;
    static unsigned int cached = 1u;

    if (sixel_kmeans_restarts_override_enabled) {
        if (sixel_kmeans_restarts_override_value < 1u) {
            return 1u;
        }
        if (sixel_kmeans_restarts_override_value > 32u) {
            return 32u;
        }
        return sixel_kmeans_restarts_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmeans_parse_env_uint("SIXEL_PALETTE_KMEANS_RESTARTS",
                                         1u,
                                         32u,
                                         0,
                                         1u,
                                         NULL);

    return cached;
}

void
sixel_set_kmeans_iter_override(int enabled,
                               unsigned int iter_count)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_iter_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_iter_override_value = iter_count;
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_iter(void)
{
    static int loaded = 0;
    static int cached_present = 0;
    static unsigned int cached = 0u;

    if (sixel_kmeans_iter_override_enabled) {
        if (sixel_kmeans_iter_override_value < 1u) {
            return 1u;
        }
        if (sixel_kmeans_iter_override_value > 100u) {
            return 100u;
        }
        return sixel_kmeans_iter_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmeans_parse_env_uint("SIXEL_PALETTE_KMEANS_ITER",
                                         1u,
                                         100u,
                                         0,
                                         0u,
                                         &cached_present);

    return cached;
}

SIXEL_INTERNAL_API int
sixel_get_kmeans_iter_enabled(void)
{
    static int loaded = 0;
    static int cached_present = 0;
    unsigned int parsed;

    parsed = 0u;
    if (sixel_kmeans_iter_override_enabled) {
        return 1;
    }
    if (loaded) {
        return cached_present;
    }
    loaded = 1;
    parsed = sixel_kmeans_parse_env_uint("SIXEL_PALETTE_KMEANS_ITER",
                                         1u,
                                         100u,
                                         0,
                                         0u,
                                         &cached_present);
    (void)parsed;

    return cached_present;
}

void
sixel_set_kmeans_miniter_override(int enabled,
                                  unsigned int miniter)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_miniter_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_miniter_override_value = miniter;
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_miniter(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmeans_miniter_override_enabled) {
        if (sixel_kmeans_miniter_override_value > 100u) {
            return 100u;
        }
        return sixel_kmeans_miniter_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmeans_parse_env_uint("SIXEL_PALETTE_KMEANS_MINITER",
                                         0u,
                                         100u,
                                         1,
                                         0u,
                                         NULL);

    return cached;
}

void
sixel_set_kmeans_polish_iter_override(int enabled,
                                      unsigned int polish_iter)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_polish_iter_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_polish_iter_override_value = polish_iter;
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_polish_iter(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmeans_polish_iter_override_enabled) {
        if (sixel_kmeans_polish_iter_override_value > 16u) {
            return 16u;
        }
        return sixel_kmeans_polish_iter_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmeans_parse_env_uint(
        "SIXEL_PALETTE_KMEANS_POLISH_ITER",
        0u,
        16u,
        1,
        0u,
        NULL);

    return cached;
}

static sixel_kmeans_feedback_mode
sixel_kmeans_resolve_feedback_mode(sixel_kmeans_feedback_mode mode)
{
    switch (mode) {
    case SIXEL_PALETTE_KMEANS_FEEDBACK_OFF:
    case SIXEL_PALETTE_KMEANS_FEEDBACK_ON:
        return mode;
    default:
        return SIXEL_PALETTE_KMEANS_FEEDBACK_OFF;
    }
}

void
sixel_set_kmeans_feedback_mode_override(int enabled,
                                        sixel_kmeans_feedback_mode mode)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_feedback_mode_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_feedback_mode_override_value
        = sixel_kmeans_resolve_feedback_mode(mode);
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kmeans_feedback_mode
sixel_get_kmeans_feedback_mode(void)
{
    char const *env_value;
    static int loaded = 0;
    static sixel_kmeans_feedback_mode cached
        = SIXEL_PALETTE_KMEANS_FEEDBACK_OFF;

    env_value = NULL;
    if (sixel_kmeans_feedback_mode_override_enabled) {
        return sixel_kmeans_feedback_mode_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_FEEDBACK");
    if (env_value != NULL && env_value[0] != '\0') {
        if (sixel_compat_strcasecmp(env_value, "on") == 0) {
            cached = SIXEL_PALETTE_KMEANS_FEEDBACK_ON;
        } else if (sixel_compat_strcasecmp(env_value, "off") == 0) {
            cached = SIXEL_PALETTE_KMEANS_FEEDBACK_OFF;
        }
    }

    return cached;
}

static char const *
sixel_kmeans_prune_policy_to_string(sixel_kmeans_prune_policy policy)
{
    switch (policy) {
    case SIXEL_PALETTE_KMEANS_PRUNE_NONE:
        return "none";
    case SIXEL_PALETTE_KMEANS_PRUNE_HAMERLY:
        return "hamerly";
    case SIXEL_PALETTE_KMEANS_PRUNE_ELKAN:
        return "elkan";
    case SIXEL_PALETTE_KMEANS_PRUNE_YINYANG:
        return "yinyang";
    case SIXEL_PALETTE_KMEANS_PRUNE_AUTO:
    default:
        return "auto";
    }
}

static sixel_kmeans_prune_policy
sixel_kmeans_resolve_prune_policy(sixel_kmeans_prune_policy policy)
{
    switch (policy) {
    case SIXEL_PALETTE_KMEANS_PRUNE_AUTO:
    case SIXEL_PALETTE_KMEANS_PRUNE_HAMERLY:
        return SIXEL_PALETTE_KMEANS_PRUNE_HAMERLY;
    case SIXEL_PALETTE_KMEANS_PRUNE_NONE:
        return SIXEL_PALETTE_KMEANS_PRUNE_NONE;
    case SIXEL_PALETTE_KMEANS_PRUNE_ELKAN:
        return SIXEL_PALETTE_KMEANS_PRUNE_ELKAN;
    case SIXEL_PALETTE_KMEANS_PRUNE_YINYANG:
        return SIXEL_PALETTE_KMEANS_PRUNE_YINYANG;
    default:
        return SIXEL_PALETTE_KMEANS_PRUNE_HAMERLY;
    }
}

void
sixel_set_kmeans_prune_policy_override(int enabled,
                                       sixel_kmeans_prune_policy policy)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_prune_policy_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_prune_policy_override_value = policy;
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kmeans_prune_policy
sixel_get_kmeans_prune_policy(void)
{
    char const *env_value;
    static int loaded = 0;
    static sixel_kmeans_prune_policy cached
        = SIXEL_PALETTE_KMEANS_PRUNE_HAMERLY;
    sixel_kmeans_prune_policy parsed;
    sixel_kmeans_prune_policy resolved;

    env_value = NULL;
    parsed = SIXEL_PALETTE_KMEANS_PRUNE_AUTO;
    resolved = SIXEL_PALETTE_KMEANS_PRUNE_HAMERLY;
    if (sixel_kmeans_prune_policy_override_enabled) {
        return sixel_kmeans_resolve_prune_policy(
            sixel_kmeans_prune_policy_override_value);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_PRUNE");
    if (env_value != NULL && env_value[0] != '\0') {
        if (sixel_compat_strcasecmp(env_value, "none") == 0) {
            parsed = SIXEL_PALETTE_KMEANS_PRUNE_NONE;
        } else if (sixel_compat_strcasecmp(env_value, "hamerly") == 0) {
            parsed = SIXEL_PALETTE_KMEANS_PRUNE_HAMERLY;
        } else if (sixel_compat_strcasecmp(env_value, "elkan") == 0) {
            parsed = SIXEL_PALETTE_KMEANS_PRUNE_ELKAN;
        } else if (sixel_compat_strcasecmp(env_value, "yinyang") == 0) {
            parsed = SIXEL_PALETTE_KMEANS_PRUNE_YINYANG;
        } else if (sixel_compat_strcasecmp(env_value, "auto") == 0) {
            parsed = SIXEL_PALETTE_KMEANS_PRUNE_AUTO;
        }
    }
    resolved = sixel_kmeans_resolve_prune_policy(parsed);
    cached = resolved;
    sixel_debugf("k-means prune policy: %s",
                 sixel_kmeans_prune_policy_to_string(parsed));
    return cached;
}

void
sixel_set_kmeans_feedback_slots_override(int enabled,
                                         unsigned int slots)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_feedback_slots_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_feedback_slots_override_value = slots;
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_feedback_slots(void)
{
    static int loaded = 0;
    static unsigned int cached = 1u;

    if (sixel_kmeans_feedback_slots_override_enabled) {
        if (sixel_kmeans_feedback_slots_override_value < 1u) {
            return 1u;
        }
        if (sixel_kmeans_feedback_slots_override_value > 16u) {
            return 16u;
        }
        return sixel_kmeans_feedback_slots_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmeans_parse_env_uint(
        "SIXEL_PALETTE_KMEANS_FEEDBACK_SLOTS",
        1u,
        16u,
        0,
        1u,
        NULL);

    return cached;
}

void
sixel_set_kmeans_feedback_interval_override(int enabled,
                                            unsigned int interval)
{
    int lock_acquired;

    lock_acquired = sixel_kmeans_override_lock_acquire();
    sixel_kmeans_feedback_interval_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_feedback_interval_override_value = interval;
    sixel_kmeans_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_feedback_interval(void)
{
    static int loaded = 0;
    static unsigned int cached = 1u;

    if (sixel_kmeans_feedback_interval_override_enabled) {
        if (sixel_kmeans_feedback_interval_override_value < 1u) {
            return 1u;
        }
        if (sixel_kmeans_feedback_interval_override_value > 64u) {
            return 64u;
        }
        return sixel_kmeans_feedback_interval_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmeans_parse_env_uint(
        "SIXEL_PALETTE_KMEANS_FEEDBACK_INTERVAL",
        1u,
        64u,
        0,
        1u,
        NULL);

    return cached;
}

static int
sixel_kmeans_projection_compare(void const *lhs, void const *rhs)
{
    sixel_kmeans_projection_entry_t const *left;
    sixel_kmeans_projection_entry_t const *right;

    left = (sixel_kmeans_projection_entry_t const *)lhs;
    right = (sixel_kmeans_projection_entry_t const *)rhs;
    if (left->projection < right->projection) {
        return -1;
    }
    if (left->projection > right->projection) {
        return 1;
    }

    return 0;
}

static int
sixel_kmeans_compute_mean(double const *samples,
                          double const *weights,
                          unsigned int sample_count,
                          double mean[3],
                          double *total_weight)
{
    unsigned int index;
    double weight;
    double weight_sum;
    double accum[3];

    index = 0U;
    weight = 1.0;
    weight_sum = 0.0;
    accum[0] = 0.0;
    accum[1] = 0.0;
    accum[2] = 0.0;
    if (samples == NULL || mean == NULL || total_weight == NULL) {
        return 1;
    }
    for (index = 0U; index < sample_count; ++index) {
        if (weights != NULL) {
            weight = weights[index];
        }
        if (weight <= 0.0) {
            continue;
        }
        weight_sum += weight;
        accum[0] += samples[index * 3U + 0U] * weight;
        accum[1] += samples[index * 3U + 1U] * weight;
        accum[2] += samples[index * 3U + 2U] * weight;
    }
    if (weight_sum <= 0.0) {
        return 1;
    }

    mean[0] = accum[0] / weight_sum;
    mean[1] = accum[1] / weight_sum;
    mean[2] = accum[2] / weight_sum;
    *total_weight = weight_sum;

    return 0;
}

static int
sixel_kmeans_compute_covariance(double const *samples,
                                double const *weights,
                                unsigned int sample_count,
                                double const mean[3],
                                double covariance[3][3])
{
    unsigned int index;
    unsigned int row;
    unsigned int col;
    double weight;
    double weight_sum;
    double centered[3];

    index = 0U;
    row = 0U;
    col = 0U;
    weight = 1.0;
    weight_sum = 0.0;
    centered[0] = 0.0;
    centered[1] = 0.0;
    centered[2] = 0.0;
    if (samples == NULL || covariance == NULL || mean == NULL) {
        return 1;
    }
    for (row = 0U; row < 3U; ++row) {
        for (col = 0U; col < 3U; ++col) {
            covariance[row][col] = 0.0;
        }
    }

    for (index = 0U; index < sample_count; ++index) {
        if (weights != NULL) {
            weight = weights[index];
        }
        if (weight <= 0.0) {
            continue;
        }
        centered[0] = samples[index * 3U + 0U] - mean[0];
        centered[1] = samples[index * 3U + 1U] - mean[1];
        centered[2] = samples[index * 3U + 2U] - mean[2];
        for (row = 0U; row < 3U; ++row) {
            for (col = row; col < 3U; ++col) {
                covariance[row][col]
                    += centered[row] * centered[col] * weight;
            }
        }
        weight_sum += weight;
    }
    if (weight_sum <= 0.0) {
        return 1;
    }
    for (row = 0U; row < 3U; ++row) {
        for (col = row; col < 3U; ++col) {
            covariance[row][col] /= weight_sum;
            if (row != col) {
                covariance[col][row] = covariance[row][col];
            }
        }
    }

    return 0;
}

static int
sixel_kmeans_power_iteration(double covariance[3][3],
                             double axis[3],
                             unsigned int iterations)
{
    double vector[3];
    double next[3];
    double norm;
    unsigned int iter;

    vector[0] = 1.0;
    vector[1] = 1.0;
    vector[2] = 1.0;
    next[0] = 0.0;
    next[1] = 0.0;
    next[2] = 0.0;
    norm = 0.0;
    for (iter = 0U; iter < iterations; ++iter) {
        next[0] = covariance[0][0] * vector[0]
            + covariance[0][1] * vector[1]
            + covariance[0][2] * vector[2];
        next[1] = covariance[1][0] * vector[0]
            + covariance[1][1] * vector[1]
            + covariance[1][2] * vector[2];
        next[2] = covariance[2][0] * vector[0]
            + covariance[2][1] * vector[1]
            + covariance[2][2] * vector[2];
        norm = next[0] * next[0]
            + next[1] * next[1]
            + next[2] * next[2];
        if (norm <= 0.0) {
            return 1;
        }
        norm = sqrt(norm);
        vector[0] = next[0] / norm;
        vector[1] = next[1] / norm;
        vector[2] = next[2] / norm;
    }

    axis[0] = vector[0];
    axis[1] = vector[1];
    axis[2] = vector[2];

    return 0;
}

static int
sixel_kmeans_seed_pca(double *centers,
                      unsigned int k,
                      double const *samples,
                      double const *weights,
                      unsigned int sample_count,
                      int use_reversible,
                      int pixelformat,
                      sixel_allocator_t *allocator)
{
    sixel_kmeans_projection_entry_t *projections = NULL;
    double covariance[3][3];
    double mean[3];
    double axis[3];
    double total_weight = 0.0;
    double bucket_start = 0.0;
    double bucket_end = 0.0;
    double bucket_weight = 0.0;
    double cumulative = 0.0;
    double sum[3] = { 0.0, 0.0, 0.0 };
    unsigned int bucket = 0U;
    unsigned int cursor = 0U;
    unsigned int channel = 0U;
    int status;

    status = sixel_kmeans_compute_mean(samples,
                                       weights,
                                       sample_count,
                                       mean,
                                       &total_weight);
    if (status != 0) {
        return 1;
    }
    status = sixel_kmeans_compute_covariance(samples,
                                             weights,
                                             sample_count,
                                             mean,
                                             covariance);
    if (status != 0) {
        return 1;
    }
    status = sixel_kmeans_power_iteration(covariance, axis, 16U);
    if (status != 0) {
        return 1;
    }

    projections = (sixel_kmeans_projection_entry_t *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_count * sizeof(sixel_kmeans_projection_entry_t));
    if (projections == NULL) {
        return 1;
    }
    for (cursor = 0U; cursor < sample_count; ++cursor) {
        double weight;

        weight = 1.0;
        if (weights != NULL) {
            weight = weights[cursor];
        }
        projections[cursor].projection
            = samples[cursor * 3U + 0U] * axis[0]
            + samples[cursor * 3U + 1U] * axis[1]
            + samples[cursor * 3U + 2U] * axis[2];
        projections[cursor].weight = weight;
        projections[cursor].index = cursor;
    }
    qsort(projections,
          (size_t)sample_count,
          sizeof(sixel_kmeans_projection_entry_t),
          sixel_kmeans_projection_compare);

    cumulative = 0.0;
    cursor = 0U;
    for (bucket = 0U; bucket < k; ++bucket) {
        bucket_start = total_weight * (double)bucket / (double)k;
        bucket_end = total_weight * (double)(bucket + 1U)
            / (double)k;
        bucket_weight = 0.0;
        sum[0] = 0.0;
        sum[1] = 0.0;
        sum[2] = 0.0;
        while (cursor < sample_count && cumulative < bucket_end) {
            double weight;
            unsigned int sample_index;

            weight = projections[cursor].weight;
            if (weight <= 0.0) {
                ++cursor;
                continue;
            }
            sample_index = projections[cursor].index;
            if (cumulative + weight < bucket_start) {
                cumulative += weight;
                ++cursor;
                continue;
            }
            bucket_weight += weight;
            sum[0] += samples[sample_index * 3U + 0U] * weight;
            sum[1] += samples[sample_index * 3U + 1U] * weight;
            sum[2] += samples[sample_index * 3U + 2U] * weight;
            cumulative += weight;
            ++cursor;
        }
        if (bucket_weight <= 0.0) {
            unsigned int fallback;

            fallback = 0U;
            if (cursor > 0U) {
                fallback = projections[cursor - 1U].index;
            }
            bucket_weight = 1.0;
            sum[0] = samples[fallback * 3U + 0U];
            sum[1] = samples[fallback * 3U + 1U];
            sum[2] = samples[fallback * 3U + 2U];
        }
        for (channel = 0U; channel < 3U; ++channel) {
            centers[bucket * 3U + channel] = sum[channel] / bucket_weight;
        }
        sixel_palette_snap_triple(&centers[bucket * 3U],
                                  use_reversible,
                                  pixelformat,
                                  SIXEL_PALETTE_SNAP_STAGE_INITIAL_SEED);
    }

    sixel_allocator_free(allocator, projections);

    return 0;
}

static uint32_t
sixel_kmeans_rng_next(uint32_t *state)
{
    uint32_t x;

    x = 0u;
    if (state == NULL) {
        return 0u;
    }
    x = *state;
    if (x == 0u) {
        x = 0x9e3779b9u;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;

    return x;
}

static double
sixel_kmeans_rng_unit(uint32_t *state)
{
    uint32_t value;

    value = 0u;
    value = sixel_kmeans_rng_next(state);

    return (double)value / 4294967296.0;
}

static SIXELSTATUS
sixel_kmeans_seed_legacy(double *centers,
                         unsigned int k,
                         double const *samples,
                         double const *weights,
                         unsigned int sample_count,
                         double *distance_cache,
                         uint32_t *rng_state)
{
    unsigned int channel;
    unsigned int index;
    unsigned int sample_index;
    unsigned int center_index;
    unsigned int replace;
    unsigned int first_index;
    double total_weight;
    double random_point;
    double distance;
    double diff;
    double sample_weight;
    double random_unit;

    channel = 0U;
    index = 0U;
    sample_index = 0U;
    center_index = 0U;
    replace = 0U;
    first_index = 0U;
    total_weight = 0.0;
    random_point = 0.0;
    distance = 0.0;
    diff = 0.0;
    sample_weight = 1.0;
    random_unit = 0.0;
    replace = 0U;
    if (weights != NULL) {
        for (sample_index = 0U; sample_index < sample_count; ++sample_index) {
            sample_weight = weights[sample_index];
            if (sample_weight > 0.0) {
                total_weight += sample_weight;
            }
        }
        if (total_weight > 0.0) {
            if (rng_state != NULL) {
                random_unit = sixel_kmeans_rng_unit(rng_state);
            } else {
                random_unit = (double)rand() / ((double)RAND_MAX + 1.0);
            }
            random_point = random_unit * total_weight;
            first_index = 0U;
            while (first_index + 1U < sample_count) {
                sample_weight = weights[first_index];
                if (sample_weight > 0.0) {
                    if (random_point <= sample_weight) {
                        break;
                    }
                    random_point -= sample_weight;
                }
                ++first_index;
            }
            replace = first_index;
        } else {
            if (rng_state != NULL) {
                replace = sixel_kmeans_rng_next(rng_state) % sample_count;
            } else {
                replace = (unsigned int)((unsigned long)rand() % sample_count);
            }
        }
    } else {
        if (rng_state != NULL) {
            replace = sixel_kmeans_rng_next(rng_state) % sample_count;
        } else {
            replace = (unsigned int)((unsigned long)rand() % sample_count);
        }
    }
    for (channel = 0U; channel < 3U; ++channel) {
        centers[channel] = (double)samples[replace * 3U + channel];
    }
    for (sample_index = 0U; sample_index < sample_count; ++sample_index) {
        distance = 0.0;
        for (channel = 0U; channel < 3U; ++channel) {
            diff = (double)samples[sample_index * 3U + channel]
                - centers[channel];
            distance += diff * diff;
        }
        distance_cache[sample_index] = distance;
    }
    for (center_index = 1U; center_index < k; ++center_index) {
        total_weight = 0.0;
        for (sample_index = 0U; sample_index < sample_count;
                ++sample_index) {
            sample_weight = 1.0;
            if (weights != NULL) {
                sample_weight = weights[sample_index];
            }
            if (sample_weight <= 0.0) {
                continue;
            }
            total_weight += distance_cache[sample_index] * sample_weight;
        }
        random_point = 0.0;
        if (total_weight > 0.0) {
            if (rng_state != NULL) {
                random_unit = sixel_kmeans_rng_unit(rng_state);
            } else {
                random_unit = (double)rand() / ((double)RAND_MAX + 1.0);
            }
            random_point = random_unit * total_weight;
        }
        sample_index = 0U;
        while (sample_index + 1U < sample_count &&
               random_point > 0.0) {
            sample_weight = 1.0;
            if (weights != NULL) {
                sample_weight = weights[sample_index];
            }
            if (sample_weight > 0.0) {
                random_point -= distance_cache[sample_index] * sample_weight;
            }
            ++sample_index;
        }
        for (channel = 0U; channel < 3U; ++channel) {
            centers[center_index * 3U + channel]
                = (double)samples[sample_index * 3U + channel];
        }
        for (index = 0U; index < sample_count; ++index) {
            distance = 0.0;
            for (channel = 0U; channel < 3U; ++channel) {
                diff = (double)samples[index * 3U + channel]
                    - centers[center_index * 3U + channel];
                distance += diff * diff;
            }
            if (distance < distance_cache[index]) {
                distance_cache[index] = distance;
            }
        }
    }

    return SIXEL_OK;
}

SIXELSTATUS
sixel_kmeans_choose_initial_centroids(double *centers,
                                      unsigned int k,
                                      double const *samples,
                                      double const *weights,
                                      unsigned int sample_count,
                                      int use_reversible,
                                      int pixelformat,
                                      double *distance_cache,
                                      sixel_allocator_t *allocator,
                                      sixel_kmeans_init_type init_type,
                                      uint32_t *rng_state)
{
    sixel_kmeans_init_type resolved;
    SIXELSTATUS status;
    double *scratch_distances;
    double snapped[3];
    unsigned int center_index;
    unsigned int channel;

    resolved = sixel_kmeans_resolve_init_type(init_type);
    status = SIXEL_BAD_ARGUMENT;
    scratch_distances = distance_cache;
    if (centers == NULL || samples == NULL || allocator == NULL) {
        return status;
    }
    if (k == 0U || sample_count == 0U) {
        return status;
    }
    if (resolved == SIXEL_PALETTE_KMEANS_INIT_PCA) {
        int seed_status;

        seed_status = sixel_kmeans_seed_pca(centers,
                                            k,
                                            samples,
                                            weights,
                                            sample_count,
                                            use_reversible,
                                            pixelformat,
                                            allocator);
        if (seed_status == 0) {
            return SIXEL_OK;
        }
        sixel_debugf("PCA seeding failed, falling back to legacy mode");
        resolved = SIXEL_PALETTE_KMEANS_INIT_NONE;
    }

    if (scratch_distances == NULL) {
        scratch_distances = (double *)sixel_allocator_malloc(
            allocator, (size_t)sample_count * sizeof(double));
        if (scratch_distances == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
    }
    status = sixel_kmeans_seed_legacy(centers,
                                      k,
                                      samples,
                                      weights,
                                      sample_count,
                                      scratch_distances,
                                      rng_state);
    if (scratch_distances != distance_cache) {
        sixel_allocator_free(allocator, scratch_distances);
    }

    /*
     * Snap initial centroids when the timing policy requests it.  This keeps
     * seed positions aligned with the reversible grid before Lloyd
     * refinement begins.
     */
    if (SIXEL_SUCCEEDED(status)) {
        for (center_index = 0U; center_index < k; ++center_index) {
            for (channel = 0U; channel < 3U; ++channel) {
                snapped[channel]
                    = centers[center_index * 3U + channel];
            }
            sixel_palette_snap_triple(
                snapped,
                use_reversible,
                pixelformat,
                SIXEL_PALETTE_SNAP_STAGE_INITIAL_SEED);
            for (channel = 0U; channel < 3U; ++channel) {
                centers[center_index * 3U + channel] = snapped[channel];
            }
        }
    }

    return status;
}
/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
