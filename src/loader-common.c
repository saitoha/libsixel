/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Shared loader helpers used across backend implementations.  This module
 * centralizes trace logging, thumbnail size hints, and small detection
 * helpers so backend files stay narrow and platform headers remain isolated.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDARG_H
# include <stdarg.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif

/* Keep SIZE_MAX available even on strict C99 environments. */
#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#include <sixel.h>

#include "cms.h"
#include "compat_stub.h"
#include "frame.h"
#include "loader-common.h"
#include "logger.h"

#if defined(__PCC__) || defined(__TINYC__)
# define SIXEL_LOADER_NO_TLS_COMPILER 1
#else
# define SIXEL_LOADER_NO_TLS_COMPILER 0
#endif

#if defined(_MSC_VER)
# define SIXEL_LOADER_TLS __declspec(thread)
# define SIXEL_LOADER_TLS_AVAILABLE 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !SIXEL_LOADER_NO_TLS_COMPILER
# define SIXEL_LOADER_TLS _Thread_local
# define SIXEL_LOADER_TLS_AVAILABLE 1
#elif (defined(__GNUC__) || defined(__clang__)) \
    && !SIXEL_LOADER_NO_TLS_COMPILER
# define SIXEL_LOADER_TLS __thread
# define SIXEL_LOADER_TLS_AVAILABLE 1
#else
# define SIXEL_LOADER_TLS
# define SIXEL_LOADER_TLS_AVAILABLE 0
#endif

static int thumbnailer_default_size_hint = SIXEL_THUMBNAILER_DEFAULT_SIZE;
static int thumbnailer_size_hint = SIXEL_THUMBNAILER_DEFAULT_SIZE;
static int thumbnailer_size_hint_initialized;
static int wic_ico_minsize_default;
static int wic_ico_minsize;
static int wic_ico_minsize_initialized;
static int libpng_enable_cms_default = 0;
static int libpng_enable_cms = 0;
static int builtin_enable_cms_default = 0;
static int builtin_enable_cms = 0;
static int loader_background_colorspace_initialized;
static int loader_background_colorspace_value = SIXEL_COLORSPACE_GAMMA;
static int loader_transparent_policy_initialized;
static int loader_transparent_policy_value =
    SIXEL_LOADER_TRANSPARENT_POLICY_COMPOSITE;
static int loader_background_policy_initialized;
static int loader_background_policy_value =
    SIXEL_LOADER_BACKGROUND_POLICY_FILE_FIRST;
/*
 * Per-thread temporary override used by loader.c when OSC11 provides
 * a terminal UI background color.
 */
static SIXEL_LOADER_TLS int loader_background_colorspace_override = -1;
static int loader_cms_target_initialized;
static int loader_cms_prefer_8bit_flag;
static int loader_cms_target_colorspace_value = SIXEL_COLORSPACE_LINEAR;

typedef struct sixel_loader_timeline_scope {
    sixel_logger_t *logger;
    char worker[96];
    int *job_seq;
    unsigned int optional_mask;
    int active;
} sixel_loader_timeline_scope_t;

static SIXEL_LOADER_TLS sixel_loader_timeline_scope_t loader_timeline_scope;

#if SIXEL_ENABLE_THREADS && !SIXEL_LOADER_TLS_AVAILABLE
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
static CRITICAL_SECTION loader_background_mutex;
static INIT_ONCE loader_background_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
loader_background_lock_init_once(PINIT_ONCE once,
                                 PVOID parameter,
                                 PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;

    InitializeCriticalSection(&loader_background_mutex);
    return TRUE;
}
# else
#  include <pthread.h>
/*
 * Use runtime initialization so Cosmopolitan/pcc builds avoid warnings from
 * partial pthread struct initializers.
 */
static pthread_mutex_t loader_background_mutex;
static pthread_once_t loader_background_mutex_once = PTHREAD_ONCE_INIT;
static int loader_background_mutex_ready = 0;

static void
loader_background_lock_init_once(void)
{
    int rc;

    rc = pthread_mutex_init(&loader_background_mutex, NULL);
    if (rc == 0) {
        loader_background_mutex_ready = 1;
    }
}
# endif

/*
 * Without TLS the override flag and default colorspace state become process
 * globals. Serialize access so concurrent loads do not race on these values.
 */
static void
loader_background_lock(void)
{
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    BOOL initialized;

    initialized = InitOnceExecuteOnce(&loader_background_once,
                                      loader_background_lock_init_once,
                                      NULL,
                                      NULL);
    if (!initialized) {
        abort();
    }
    EnterCriticalSection(&loader_background_mutex);
# else
    int rc;
    int once_status;

    once_status = pthread_once(&loader_background_mutex_once,
                               loader_background_lock_init_once);
    if (once_status != 0 || !loader_background_mutex_ready) {
        abort();
    }

    rc = pthread_mutex_lock(&loader_background_mutex);
    if (rc != 0) {
        abort();
    }
# endif
}

static void
loader_background_unlock(void)
{
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    LeaveCriticalSection(&loader_background_mutex);
# else
    int rc;

    if (!loader_background_mutex_ready) {
        abort();
    }
    rc = pthread_mutex_unlock(&loader_background_mutex);
    if (rc != 0) {
        abort();
    }
# endif
}
#else
static void
loader_background_lock(void)
{
}

static void
loader_background_unlock(void)
{
}
#endif

#undef SIXEL_LOADER_TLS
#undef SIXEL_LOADER_TLS_AVAILABLE
#undef SIXEL_LOADER_NO_TLS_COMPILER

#define SIXEL_ENV_WIC_ICO_MINSIZE "SIXEL_LOADER_WIC_ICO_MINSIZE"
#define SIXEL_ENV_WIC_ICO_MINSIZE_LEGACY "SIXEL_LODER_WIC_ICO_MINSIZE"

#define SIXEL_LOADER_TIMELINE_OPT_COLORSPACE (1u << 0)
#define SIXEL_LOADER_TIMELINE_OPT_BACKGROUND (1u << 1)
#define SIXEL_LOADER_TIMELINE_OPT_ICC        (1u << 2)
#define SIXEL_LOADER_TIMELINE_CB_MAGIC       0x534c544dU

static unsigned int
loader_timeline_optional_bit(char const *role)
{
    if (role == NULL) {
        return 0u;
    }
    if (strcmp(role, "post/colorspace") == 0) {
        return SIXEL_LOADER_TIMELINE_OPT_COLORSPACE;
    }
    if (strcmp(role, "post/background") == 0) {
        return SIXEL_LOADER_TIMELINE_OPT_BACKGROUND;
    }
    if (strcmp(role, "post/icc") == 0) {
        return SIXEL_LOADER_TIMELINE_OPT_ICC;
    }
    return 0u;
}

static int
loader_common_timeline_next_job(void)
{
    int job_id;

    job_id = -1;
    loader_background_lock();
    if (!loader_timeline_scope.active
        || loader_timeline_scope.job_seq == NULL) {
        loader_background_unlock();
        return -1;
    }
    if (*loader_timeline_scope.job_seq < 0) {
        *loader_timeline_scope.job_seq = 0;
    }
    job_id = *loader_timeline_scope.job_seq;
    *loader_timeline_scope.job_seq = job_id + 1;
    loader_background_unlock();
    return job_id;
}

static void
loader_timeline_log_event(char const *role, char const *event, int job_id)
{
    sixel_logger_t *logger;
    char worker[96];
    size_t worker_length;

    logger = NULL;
    worker[0] = '\0';
    worker_length = 0u;
    loader_background_lock();
    if (!loader_timeline_scope.active || loader_timeline_scope.logger == NULL ||
        role == NULL || event == NULL || job_id < 0) {
        loader_background_unlock();
        return;
    }
    logger = loader_timeline_scope.logger;
    worker_length = strlen(loader_timeline_scope.worker);
    if (worker_length >= sizeof(worker)) {
        worker_length = sizeof(worker) - 1u;
    }
    memcpy(worker, loader_timeline_scope.worker, worker_length);
    worker[worker_length] = '\0';
    loader_background_unlock();

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
loader_wic_initialize_ico_minsize(void)
{
    char const *env_value;
    char *endptr;
    long parsed;

    loader_background_lock();
    if (wic_ico_minsize_initialized) {
        loader_background_unlock();
        return;
    }

    wic_ico_minsize_initialized = 1;
    wic_ico_minsize_default = 0;
    wic_ico_minsize = 0;

    env_value = sixel_compat_getenv(SIXEL_ENV_WIC_ICO_MINSIZE);
    if (env_value == NULL || env_value[0] == '\0') {
        env_value = sixel_compat_getenv(SIXEL_ENV_WIC_ICO_MINSIZE_LEGACY);
    }
    if (env_value == NULL || env_value[0] == '\0') {
        loader_background_unlock();
        return;
    }

    errno = 0;
    parsed = strtol(env_value, &endptr, 10);
    if (errno != 0) {
        loader_background_unlock();
        return;
    }
    if (endptr == env_value || *endptr != '\0') {
        loader_background_unlock();
        return;
    }
    if (parsed <= 0) {
        loader_background_unlock();
        return;
    }
    if (parsed > (long)INT_MAX) {
        parsed = (long)INT_MAX;
    }

    wic_ico_minsize_default = (int)parsed;
    wic_ico_minsize = wic_ico_minsize_default;
    loader_background_unlock();
}

int
loader_wic_get_ico_minsize(void)
{
    int value;

    loader_wic_initialize_ico_minsize();
    loader_background_lock();
    value = wic_ico_minsize;
    loader_background_unlock();

    return value;
}

void
sixel_helper_set_wic_ico_minsize(int size)
{
    loader_wic_initialize_ico_minsize();

    loader_background_lock();
    if (size > 0) {
        wic_ico_minsize = size;
    } else {
        wic_ico_minsize = wic_ico_minsize_default;
    }
    loader_background_unlock();
}

int
loader_libpng_get_enable_cms(void)
{
    int value;

    loader_background_lock();
    value = libpng_enable_cms;
    loader_background_unlock();

    return value;
}

void
sixel_helper_set_libpng_enable_cms(int enable)
{
    loader_background_lock();
    if (enable >= 0) {
        libpng_enable_cms = enable != 0 ? 1 : 0;
    } else {
        libpng_enable_cms = libpng_enable_cms_default;
    }
    loader_background_unlock();
}

int
loader_builtin_get_enable_cms(void)
{
    int value;

    loader_background_lock();
    value = builtin_enable_cms;
    loader_background_unlock();

    return value;
}

void
sixel_helper_set_builtin_enable_cms(int enable)
{
    loader_background_lock();
    if (enable >= 0) {
        builtin_enable_cms = enable != 0 ? 1 : 0;
    } else {
        builtin_enable_cms = builtin_enable_cms_default;
    }
    loader_background_unlock();
}

SIXEL_INTERNAL_API void
sixel_helper_set_loader_background_colorspace(int colorspace)
{
    loader_background_lock();
    if (colorspace == SIXEL_COLORSPACE_GAMMA ||
            colorspace == SIXEL_COLORSPACE_LINEAR) {
        loader_background_colorspace_override = colorspace;
    } else {
        loader_background_colorspace_override = -1;
    }
    loader_background_unlock();
}

static void
loader_background_initialize_colorspace(void)
{
    char const *env_value;

    loader_background_lock();
    if (loader_background_colorspace_initialized) {
        loader_background_unlock();
        return;
    }

    loader_background_colorspace_initialized = 1;
    loader_background_colorspace_value = SIXEL_COLORSPACE_GAMMA;

    env_value = sixel_compat_getenv("SIXEL_LOADER_BACKGROUND_COLORSPACE");
    if (env_value == NULL || env_value[0] == '\0') {
        loader_background_unlock();
        return;
    }

    if (strcmp(env_value, "linear") == 0) {
        loader_background_colorspace_value = SIXEL_COLORSPACE_LINEAR;
    } else if (strcmp(env_value, "gamma") == 0) {
        loader_background_colorspace_value = SIXEL_COLORSPACE_GAMMA;
    }
    loader_background_unlock();
}

static void
loader_initialize_transparent_policy(void)
{
    char const *env_value;

    loader_background_lock();
    if (loader_transparent_policy_initialized) {
        loader_background_unlock();
        return;
    }
    loader_transparent_policy_initialized = 1;
    loader_transparent_policy_value = SIXEL_LOADER_TRANSPARENT_POLICY_COMPOSITE;
    env_value = sixel_compat_getenv("SIXEL_TRANSPARENT_POLICY");
    if (env_value != NULL && env_value[0] != '\0') {
        if (strcmp(env_value, "composite") == 0) {
            loader_transparent_policy_value =
                SIXEL_LOADER_TRANSPARENT_POLICY_COMPOSITE;
        } else if (strcmp(env_value, "transparent") == 0) {
            loader_transparent_policy_value =
                SIXEL_LOADER_TRANSPARENT_POLICY_TRANSPARENT;
        }
    }
    loader_background_unlock();
}

static void
loader_initialize_background_policy(void)
{
    char const *env_value;

    loader_background_lock();
    if (loader_background_policy_initialized) {
        loader_background_unlock();
        return;
    }
    loader_background_policy_initialized = 1;
    loader_background_policy_value = SIXEL_LOADER_BACKGROUND_POLICY_FILE_FIRST;
    env_value = sixel_compat_getenv("SIXEL_BACKGROUND_POLICY");
    if (env_value != NULL && env_value[0] != '\0') {
        if (strcmp(env_value, "file_first") == 0) {
            loader_background_policy_value =
                SIXEL_LOADER_BACKGROUND_POLICY_FILE_FIRST;
        } else if (strcmp(env_value, "explicit_first") == 0) {
            loader_background_policy_value =
                SIXEL_LOADER_BACKGROUND_POLICY_EXPLICIT_FIRST;
        }
    }
    loader_background_unlock();
}

SIXEL_INTERNAL_API int
loader_background_colorspace(void)
{
    int override_value;

    loader_background_lock();
    override_value = loader_background_colorspace_override;
    if (override_value == SIXEL_COLORSPACE_GAMMA ||
            override_value == SIXEL_COLORSPACE_LINEAR) {
        loader_background_unlock();
        return override_value;
    }
    loader_background_unlock();

    loader_background_initialize_colorspace();

    loader_background_lock();
    override_value = loader_background_colorspace_value;
    loader_background_unlock();

    return override_value;
}

int
loader_transparent_policy(void)
{
    int policy;

    loader_initialize_transparent_policy();
    loader_background_lock();
    policy = loader_transparent_policy_value;
    loader_background_unlock();

    return policy;
}

SIXEL_INTERNAL_API int
loader_background_policy(void)
{
    int policy;

    loader_initialize_background_policy();
    loader_background_lock();
    policy = loader_background_policy_value;
    loader_background_unlock();

    return policy;
}

static void
loader_cms_initialize_target(void)
{
    char const *prefer8_env;
    char const *target_env;

    loader_background_lock();
    if (loader_cms_target_initialized) {
        loader_background_unlock();
        return;
    }
    loader_cms_target_initialized = 1;
    loader_cms_prefer_8bit_flag = 0;
    loader_cms_target_colorspace_value = SIXEL_COLORSPACE_LINEAR;

    prefer8_env = sixel_compat_getenv("SIXEL_LOADER_PREFER_8BIT");
    if (prefer8_env != NULL && strcmp(prefer8_env, "1") == 0) {
        loader_cms_prefer_8bit_flag = 1;
    }

    target_env = sixel_compat_getenv("SIXEL_LOADER_CMS_TARGET_COLORSPACE");
    if (target_env == NULL || target_env[0] == '\0') {
        loader_background_unlock();
        return;
    }
    if (strcmp(target_env, "gamma") == 0) {
        loader_cms_target_colorspace_value = SIXEL_COLORSPACE_GAMMA;
    } else if (strcmp(target_env, "linear") == 0) {
        loader_cms_target_colorspace_value = SIXEL_COLORSPACE_LINEAR;
    } else if (strcmp(target_env, "cielab") == 0) {
        loader_cms_target_colorspace_value = SIXEL_COLORSPACE_CIELAB;
    } else if (strcmp(target_env, "oklab") == 0) {
        loader_cms_target_colorspace_value = SIXEL_COLORSPACE_OKLAB;
    } else if (strcmp(target_env, "din99d") == 0) {
        loader_cms_target_colorspace_value = SIXEL_COLORSPACE_DIN99D;
    }
    loader_background_unlock();
}

int
loader_cms_prefer_8bit(void)
{
    int prefer_8bit;

    loader_cms_initialize_target();
    loader_background_lock();
    prefer_8bit = loader_cms_prefer_8bit_flag;
    loader_background_unlock();

    return prefer_8bit;
}

int
loader_cms_target_colorspace(void)
{
    int target_colorspace;

    loader_cms_initialize_target();
    loader_background_lock();
    target_colorspace = loader_cms_target_colorspace_value;
    loader_background_unlock();

    return target_colorspace;
}

SIXELAPI int
loader_cms_target_pixelformat(void)
{
    int prefer_8bit;
    int target_colorspace;

    loader_cms_initialize_target();

    loader_background_lock();
    prefer_8bit = loader_cms_prefer_8bit_flag;
    target_colorspace = loader_cms_target_colorspace_value;
    loader_background_unlock();

    if (prefer_8bit) {
        return SIXEL_PIXELFORMAT_RGB888;
    }
    switch (target_colorspace) {
    case SIXEL_COLORSPACE_GAMMA:
        return SIXEL_PIXELFORMAT_RGBFLOAT32;
    case SIXEL_COLORSPACE_CIELAB:
        return SIXEL_PIXELFORMAT_CIELABFLOAT32;
    case SIXEL_COLORSPACE_OKLAB:
        return SIXEL_PIXELFORMAT_OKLABFLOAT32;
    case SIXEL_COLORSPACE_DIN99D:
        return SIXEL_PIXELFORMAT_DIN99DFLOAT32;
    case SIXEL_COLORSPACE_LINEAR:
    default:
        return SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    }
}

void
sixel_helper_set_loader_cms_engine(int engine)
{
    if (engine < 0) {
        sixel_cms_set_engine(SIXEL_CMS_ENGINE_AUTO);
        return;
    }

    sixel_cms_set_engine((sixel_cms_engine_t)engine);
}

static unsigned short
loader_exif_read_u16(unsigned char const *data, int little_endian)
{
    if (data == NULL) {
        return 0u;
    }
    if (little_endian) {
        return (unsigned short)((unsigned short)data[0] |
                                (unsigned short)data[1] << 8u);
    }

    return (unsigned short)((unsigned short)data[0] << 8u |
                            (unsigned short)data[1]);
}

static unsigned int
loader_exif_read_u32(unsigned char const *data, int little_endian)
{
    if (data == NULL) {
        return 0u;
    }
    if (little_endian) {
        return (unsigned int)data[0] |
               ((unsigned int)data[1] << 8u) |
               ((unsigned int)data[2] << 16u) |
               ((unsigned int)data[3] << 24u);
    }

    return ((unsigned int)data[0] << 24u) |
           ((unsigned int)data[1] << 16u) |
           ((unsigned int)data[2] << 8u) |
           (unsigned int)data[3];
}

int
loader_exif_parse_orientation(unsigned char const *data,
                              size_t size,
                              int *orientation)
{
    int little_endian;
    unsigned short magic;
    unsigned int ifd_offset;
    unsigned short entry_count;
    size_t entries_offset;
    size_t index;
    unsigned short tag;
    unsigned short type;
    unsigned int count;
    unsigned int value_field;
    int parsed_orientation;
    size_t max_entries;

    little_endian = 0;
    magic = 0u;
    ifd_offset = 0u;
    entry_count = 0u;
    entries_offset = 0u;
    index = 0u;
    tag = 0u;
    type = 0u;
    count = 0u;
    value_field = 0u;
    parsed_orientation = 0;
    max_entries = 0u;

    if (data == NULL || orientation == NULL) {
        return 0;
    }

    if (size >= 6u && memcmp(data, "Exif\0\0", 6u) == 0) {
        data += 6u;
        size -= 6u;
    }

    if (size < 8u) {
        return 0;
    }
    if (data[0] == (unsigned char)'I' && data[1] == (unsigned char)'I') {
        little_endian = 1;
    } else if (data[0] == (unsigned char)'M' &&
               data[1] == (unsigned char)'M') {
        little_endian = 0;
    } else {
        return 0;
    }

    magic = loader_exif_read_u16(data + 2u, little_endian);
    if (magic != 42u) {
        return 0;
    }

    ifd_offset = loader_exif_read_u32(data + 4u, little_endian);
    /*
     * TIFF IFD offsets are relative to the TIFF header start and must
     * reference data after the 8-byte header itself. Offsets before this
     * point are malformed and can cause unbounded entry scans.
     */
    if (ifd_offset < 8u || (size_t)ifd_offset > size - 2u) {
        return 0;
    }
    entry_count = loader_exif_read_u16(data + ifd_offset, little_endian);
    entries_offset = (size_t)ifd_offset + 2u;
    if (entries_offset > size) {
        return 0;
    }
    /*
     * Validate entry_count without subtracting untrusted values from size.
     * This avoids unsigned underflow when entry_count is corrupted.
     */
    max_entries = (size - entries_offset) / 12u;
    if ((size_t)entry_count > max_entries) {
        return 0;
    }

    for (index = 0u; index < (size_t)entry_count; ++index) {
        unsigned char const *entry;

        entry = data + entries_offset + index * 12u;
        tag = loader_exif_read_u16(entry, little_endian);
        if (tag != 0x0112u) {
            continue;
        }

        type = loader_exif_read_u16(entry + 2u, little_endian);
        count = loader_exif_read_u32(entry + 4u, little_endian);
        value_field = loader_exif_read_u32(entry + 8u, little_endian);
        if (type != 3u || count == 0u) {
            return 0;
        }

        if (count == 1u) {
            if (little_endian) {
                parsed_orientation = (int)(value_field & 0xffffu);
            } else {
                parsed_orientation = (int)((value_field >> 16u) & 0xffffu);
            }
        } else {
            if ((size_t)value_field > size - 2u) {
                return 0;
            }
            parsed_orientation = (int)loader_exif_read_u16(
                data + value_field,
                little_endian);
        }

        if (parsed_orientation < 1 || parsed_orientation > 8) {
            return 0;
        }

        *orientation = parsed_orientation;
        return 1;
    }

    return 0;
}

static void
loader_exif_map_coordinates(int orientation,
                            int src_width,
                            int src_height,
                            int dst_x,
                            int dst_y,
                            int *src_x,
                            int *src_y)
{
    int mapped_x;
    int mapped_y;

    mapped_x = dst_x;
    mapped_y = dst_y;
    switch (orientation) {
    case 2:
        mapped_x = src_width - 1 - dst_x;
        mapped_y = dst_y;
        break;
    case 3:
        mapped_x = src_width - 1 - dst_x;
        mapped_y = src_height - 1 - dst_y;
        break;
    case 4:
        mapped_x = dst_x;
        mapped_y = src_height - 1 - dst_y;
        break;
    case 5:
        mapped_x = dst_y;
        mapped_y = dst_x;
        break;
    case 6:
        mapped_x = dst_y;
        mapped_y = src_height - 1 - dst_x;
        break;
    case 7:
        mapped_x = src_width - 1 - dst_y;
        mapped_y = src_height - 1 - dst_x;
        break;
    case 8:
        mapped_x = src_width - 1 - dst_y;
        mapped_y = dst_x;
        break;
    case 1:
    default:
        mapped_x = dst_x;
        mapped_y = dst_y;
        break;
    }

    if (src_x != NULL) {
        *src_x = mapped_x;
    }
    if (src_y != NULL) {
        *src_y = mapped_y;
    }
}

SIXELSTATUS
loader_frame_apply_orientation(sixel_frame_t *frame,
                               int orientation)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    int src_width;
    int src_height;
    int dst_width;
    int dst_height;
    size_t src_pixel_count;
    size_t dst_pixel_count;
    int depth;
    int channels;
    int x;
    int y;
    int src_x;
    int src_y;
    size_t src_index;
    size_t dst_index;
    unsigned char *src_bytes;
    unsigned char *dst_bytes;
    float *src_floats;
    float *dst_floats;
    unsigned char *src_mask;
    unsigned char *dst_mask;
    size_t pixel_stride;

    status = SIXEL_OK;
    allocator = NULL;
    src_width = 0;
    src_height = 0;
    dst_width = 0;
    dst_height = 0;
    src_pixel_count = 0u;
    dst_pixel_count = 0u;
    depth = 0;
    channels = 0;
    x = 0;
    y = 0;
    src_x = 0;
    src_y = 0;
    src_index = 0u;
    dst_index = 0u;
    src_bytes = NULL;
    dst_bytes = NULL;
    src_floats = NULL;
    dst_floats = NULL;
    src_mask = NULL;
    dst_mask = NULL;
    pixel_stride = 0u;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (orientation <= 1 || orientation > 8) {
        return SIXEL_OK;
    }
    if (frame->width <= 0 || frame->height <= 0 || frame->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    src_width = frame->width;
    src_height = frame->height;
    if ((size_t)src_width > SIZE_MAX / (size_t)src_height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    src_pixel_count = (size_t)src_width * (size_t)src_height;
    if (orientation >= 5 && orientation <= 8) {
        dst_width = src_height;
        dst_height = src_width;
    } else {
        dst_width = src_width;
        dst_height = src_height;
    }
    if ((size_t)dst_width > SIZE_MAX / (size_t)dst_height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    dst_pixel_count = (size_t)dst_width * (size_t)dst_height;
    allocator = frame->allocator;
    depth = sixel_helper_compute_depth(frame->pixelformat);
    if (depth <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(frame->pixelformat)) {
        if (depth <= 0 || depth % (int)sizeof(float) != 0) {
            return SIXEL_BAD_ARGUMENT;
        }
        channels = depth / (int)sizeof(float);
        if (channels <= 0) {
            return SIXEL_BAD_ARGUMENT;
        }
        if (frame->pixels.f32ptr == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        if ((size_t)channels > SIZE_MAX / sizeof(float)) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        pixel_stride = (size_t)depth;
        if (dst_pixel_count > SIZE_MAX / pixel_stride) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }

        dst_floats = (float *)sixel_allocator_malloc(allocator,
                                                     dst_pixel_count *
                                                     pixel_stride);
        if (dst_floats == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        src_floats = frame->pixels.f32ptr;
        for (y = 0; y < dst_height; ++y) {
            for (x = 0; x < dst_width; ++x) {
                loader_exif_map_coordinates(orientation,
                                            src_width,
                                            src_height,
                                            x,
                                            y,
                                            &src_x,
                                            &src_y);
                src_index = (size_t)src_y * (size_t)src_width + (size_t)src_x;
                dst_index = (size_t)y * (size_t)dst_width + (size_t)x;
                memcpy((unsigned char *)(dst_floats + dst_index * channels),
                       (unsigned char const *)(src_floats +
                                               src_index * channels),
                       pixel_stride);
            }
        }
        frame->pixels.f32ptr = dst_floats;
        sixel_allocator_free(allocator, src_floats);
    } else {
        if (frame->pixelformat == SIXEL_PIXELFORMAT_PAL1 ||
            frame->pixelformat == SIXEL_PIXELFORMAT_PAL2 ||
            frame->pixelformat == SIXEL_PIXELFORMAT_PAL4 ||
            frame->pixelformat == SIXEL_PIXELFORMAT_G1 ||
            frame->pixelformat == SIXEL_PIXELFORMAT_G2 ||
            frame->pixelformat == SIXEL_PIXELFORMAT_G4) {
            return SIXEL_BAD_ARGUMENT;
        }
        if (depth <= 0) {
            return SIXEL_BAD_ARGUMENT;
        }
        if (frame->pixels.u8ptr == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        pixel_stride = (size_t)depth;
        if (dst_pixel_count > SIZE_MAX / pixel_stride) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }

        dst_bytes = (unsigned char *)sixel_allocator_malloc(allocator,
                                                             dst_pixel_count *
                                                             pixel_stride);
        if (dst_bytes == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        src_bytes = frame->pixels.u8ptr;
        for (y = 0; y < dst_height; ++y) {
            for (x = 0; x < dst_width; ++x) {
                loader_exif_map_coordinates(orientation,
                                            src_width,
                                            src_height,
                                            x,
                                            y,
                                            &src_x,
                                            &src_y);
                src_index = (size_t)src_y * (size_t)src_width + (size_t)src_x;
                dst_index = (size_t)y * (size_t)dst_width + (size_t)x;
                memcpy(dst_bytes + dst_index * pixel_stride,
                       src_bytes + src_index * pixel_stride,
                       pixel_stride);
            }
        }
        frame->pixels.u8ptr = dst_bytes;
        sixel_allocator_free(allocator, src_bytes);
    }

    src_mask = frame->transparent_mask;
    dst_mask = NULL;
    if (src_mask != NULL) {
        if (frame->transparent_mask_size != src_pixel_count) {
            sixel_allocator_free(allocator, src_mask);
            frame->transparent_mask = NULL;
            frame->transparent_mask_size = 0u;
        } else {
            dst_mask = (unsigned char *)sixel_allocator_malloc(allocator,
                                                               dst_pixel_count);
            if (dst_mask == NULL) {
                return SIXEL_BAD_ALLOCATION;
            }
            for (y = 0; y < dst_height; ++y) {
                for (x = 0; x < dst_width; ++x) {
                    loader_exif_map_coordinates(orientation,
                                                src_width,
                                                src_height,
                                                x,
                                                y,
                                                &src_x,
                                                &src_y);
                    src_index = (size_t)src_y * (size_t)src_width +
                                (size_t)src_x;
                    dst_index = (size_t)y * (size_t)dst_width + (size_t)x;
                    dst_mask[dst_index] = src_mask[src_index];
                }
            }
            frame->transparent_mask = dst_mask;
            frame->transparent_mask_size = dst_pixel_count;
            sixel_allocator_free(allocator, src_mask);
        }
    }

    frame->width = dst_width;
    frame->height = dst_height;

    return status;
}

void
loader_thumbnailer_initialize_size_hint(void)
{
    char const *env_value;
    char *endptr;
    long parsed;

    loader_background_lock();
    if (thumbnailer_size_hint_initialized) {
        loader_background_unlock();
        return;
    }

    thumbnailer_size_hint_initialized = 1;
    thumbnailer_default_size_hint = SIXEL_THUMBNAILER_DEFAULT_SIZE;
    thumbnailer_size_hint = thumbnailer_default_size_hint;

    env_value = sixel_compat_getenv("SIXEL_THUMBNAILER_HINT_SIZE");
    if (env_value == NULL || env_value[0] == '\0') {
        loader_background_unlock();
        return;
    }

    errno = 0;
    parsed = strtol(env_value, &endptr, 10);
    if (errno != 0) {
        loader_background_unlock();
        return;
    }
    if (endptr == env_value || *endptr != '\0') {
        loader_background_unlock();
        return;
    }
    if (parsed <= 0) {
        loader_background_unlock();
        return;
    }
    if (parsed > (long)INT_MAX) {
        parsed = (long)INT_MAX;
    }

    thumbnailer_default_size_hint = (int)parsed;
    thumbnailer_size_hint = thumbnailer_default_size_hint;
    loader_background_unlock();
}

int
loader_thumbnailer_get_size_hint(void)
{
    int size_hint;

    loader_thumbnailer_initialize_size_hint();
    loader_background_lock();
    size_hint = thumbnailer_size_hint;
    loader_background_unlock();

    return size_hint;
}

int
loader_thumbnailer_get_default_size_hint(void)
{
    int default_size_hint;

    loader_thumbnailer_initialize_size_hint();
    loader_background_lock();
    default_size_hint = thumbnailer_default_size_hint;
    loader_background_unlock();

    return default_size_hint;
}

SIXEL_INTERNAL_API void
sixel_helper_set_thumbnail_size_hint(int size)
{
    loader_thumbnailer_initialize_size_hint();

    loader_background_lock();
    if (size > 0) {
        thumbnailer_size_hint = size;
    } else {
        thumbnailer_size_hint = thumbnailer_default_size_hint;
    }
    loader_background_unlock();
}

void
loader_trace_message(char const *format, ...)
{
    va_list args;

    if (!loader_trace_is_enabled()) {
        return;
    }

    fprintf(stderr, "libsixel: ");

    va_start(args, format);
    sixel_compat_vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}


/*
 * Return non-zero when SIXEL_TRACE_TOPIC contains the given token.
 * Supported separators are comma, colon, semicolon, and whitespace.
 */
int
sixel_trace_topic_is_enabled(char const *topic)
{
    char const *topics;
    char const *cursor;
    char const *token_end;
    size_t topic_length;
    size_t token_length;

    topics = NULL;
    cursor = NULL;
    token_end = NULL;
    topic_length = 0u;
    token_length = 0u;

    if (topic == NULL || topic[0] == '\0') {
        return 0;
    }

    topic_length = strlen(topic);
    if (topic_length == 0u) {
        return 0;
    }

    topics = sixel_compat_getenv("SIXEL_TRACE_TOPIC");
    if (topics == NULL || topics[0] == '\0') {
        return 0;
    }

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

/* Emit topic-scoped diagnostics selected through SIXEL_TRACE_TOPIC. */
void
sixel_trace_topic_message(
    char const *topic,
    char const *format,
    ...)
{
    va_list args;

    if (!sixel_trace_topic_is_enabled(topic)) {
        return;
    }

    fprintf(stderr,
            "libsixel[%s]: ",
            topic != NULL && topic[0] != '\0' ? topic : "trace");

    va_start(args, format);
    sixel_compat_vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
    /*
     * Some runtimes keep stderr fully buffered when redirected to pipes.
     * Flush trace lines so test_runner token waits do not stall forever.
     */
    fflush(stderr);
}

void
loader_trace_try(char const *name)
{
    char const *loader_name;

    if (loader_trace_is_enabled()) {
        loader_name = name != NULL && name[0] != '\0' ? name : "unknown";
        fprintf(stderr, "libsixel: trying %s loader\n", loader_name);
        fprintf(stderr,
                "LSXLOAD1|event=try|loader=%s|code=L_TRY\n",
                loader_name);
    }
}

static char const *
loader_trace_status_code(SIXELSTATUS status)
{
    switch (status) {
    case SIXEL_OK:
        return "L_OK";
    case SIXEL_INTERRUPTED:
        return "L_INTERRUPTED";
    case SIXEL_BAD_ALLOCATION:
        return "L_ERR_BAD_ALLOCATION";
    case SIXEL_BAD_ARGUMENT:
        return "L_ERR_BAD_ARGUMENT";
    case SIXEL_BAD_INPUT:
        return "L_ERR_BAD_INPUT";
    case SIXEL_BAD_INTEGER_OVERFLOW:
        return "L_ERR_BAD_INTEGER_OVERFLOW";
    case SIXEL_BAD_FILE_PATH:
        return "L_ERR_BAD_FILE_PATH";
    case SIXEL_BAD_CLIPBOARD:
        return "L_ERR_BAD_CLIPBOARD";
    case SIXEL_LOADER_FAILED:
        return "L_ERR_LOADER_FAILED";
    case SIXEL_NOT_IMPLEMENTED:
        return "L_ERR_NOT_IMPLEMENTED";
    case SIXEL_FALSE:
        return "L_ERR_FALSE";
    case SIXEL_RUNTIME_ERROR:
        return "L_ERR_RUNTIME";
    case SIXEL_LOGIC_ERROR:
        return "L_ERR_LOGIC";
    case SIXEL_FEATURE_ERROR:
        return "L_ERR_FEATURE";
    case SIXEL_LIBC_ERROR:
        return "L_ERR_LIBC";
    case SIXEL_CURL_ERROR:
        return "L_ERR_CURL";
    case SIXEL_JPEG_ERROR:
        return "L_ERR_JPEG";
    case SIXEL_PNG_ERROR:
        return "L_ERR_PNG";
    case SIXEL_WEBP_ERROR:
        return "L_ERR_WEBP";
    case SIXEL_TIFF_ERROR:
        return "L_ERR_TIFF";
    case SIXEL_GDK_ERROR:
        return "L_ERR_GDK";
    case SIXEL_GD_ERROR:
        return "L_ERR_GD";
    case SIXEL_STBI_ERROR:
        return "L_ERR_STBI";
    case SIXEL_STBIW_ERROR:
        return "L_ERR_STBIW";
    case SIXEL_COM_ERROR:
        return "L_ERR_COM";
    case SIXEL_WIC_ERROR:
        return "L_ERR_WIC";
    default:
        break;
    }

    return "L_ERR_UNKNOWN";
}

void
loader_trace_result(char const *name, SIXELSTATUS status)
{
    char const *event;
    char const *loader_name;

    if (!loader_trace_is_enabled()) {
        return;
    }
    loader_name = name != NULL && name[0] != '\0' ? name : "unknown";
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "libsixel: loader %s succeeded\n", loader_name);
    } else {
        fprintf(stderr, "libsixel: loader %s failed (%s)\n",
                loader_name, sixel_helper_format_error(status));
    }
    event = SIXEL_SUCCEEDED(status) ? "ok" : "fail";
    fprintf(stderr,
            "LSXLOAD1|event=%s|loader=%s|code=%s\n",
            event,
            loader_name,
            loader_trace_status_code(status));
}

int
loader_trace_is_enabled(void)
{
    return sixel_trace_topic_is_enabled("loader");
}

void
loader_timeline_scope_begin(sixel_logger_t *logger,
                            char const *worker,
                            int *job_seq)
{
    size_t worker_length;

    loader_background_lock();
    loader_timeline_scope.logger = NULL;
    loader_timeline_scope.worker[0] = '\0';
    loader_timeline_scope.job_seq = NULL;
    loader_timeline_scope.optional_mask = 0u;
    loader_timeline_scope.active = 0;

    if (logger == NULL || worker == NULL || worker[0] == '\0' ||
            job_seq == NULL || !logger->active) {
        loader_background_unlock();
        return;
    }

    worker_length = strlen(worker);
    if (worker_length >= sizeof(loader_timeline_scope.worker)) {
        worker_length = sizeof(loader_timeline_scope.worker) - 1u;
    }
    memcpy(loader_timeline_scope.worker, worker, worker_length);
    loader_timeline_scope.worker[worker_length] = '\0';
    loader_timeline_scope.logger = logger;
    loader_timeline_scope.job_seq = job_seq;
    loader_timeline_scope.optional_mask = 0u;
    loader_timeline_scope.active = 1;
    loader_background_unlock();
}

void
loader_timeline_scope_end(void)
{
    loader_background_lock();
    loader_timeline_scope.logger = NULL;
    loader_timeline_scope.worker[0] = '\0';
    loader_timeline_scope.job_seq = NULL;
    loader_timeline_scope.optional_mask = 0u;
    loader_timeline_scope.active = 0;
    loader_background_unlock();
}

int
loader_timeline_phase_start(char const *role)
{
    int job_id;

    job_id = loader_common_timeline_next_job();
    if (job_id < 0 || role == NULL) {
        return -1;
    }

    loader_timeline_log_event(role, "start", job_id);
    return job_id;
}

void
loader_timeline_phase_finish(char const *role,
                             int job_id,
                             SIXELSTATUS status)
{
    char const *event;

    if (role == NULL || job_id < 0 || !loader_timeline_scope.active) {
        return;
    }
    event = SIXEL_SUCCEEDED(status) ? "finish" : "fail";
    loader_timeline_log_event(role, event, job_id);
}

void
loader_timeline_optional_mark(char const *role)
{
    unsigned int bit;
    int job_id;

    if (!loader_timeline_scope.active) {
        return;
    }
    bit = loader_timeline_optional_bit(role);
    if (bit == 0u || (loader_timeline_scope.optional_mask & bit) != 0u) {
        return;
    }

    job_id = loader_timeline_phase_start(role);
    if (job_id >= 0) {
        loader_timeline_log_event(role, "finish", job_id);
    }
    loader_timeline_scope.optional_mask |= bit;
}

void
loader_timeline_optional_skip_if_unmarked(char const *role)
{
    unsigned int bit;
    int job_id;

    if (!loader_timeline_scope.active) {
        return;
    }
    bit = loader_timeline_optional_bit(role);
    if (bit == 0u || (loader_timeline_scope.optional_mask & bit) != 0u) {
        return;
    }

    job_id = loader_common_timeline_next_job();
    if (job_id >= 0) {
        loader_timeline_log_event(role, "skip", job_id);
    }
    loader_timeline_scope.optional_mask |= bit;
}

void
loader_timeline_callback_state_init(
    sixel_loader_timeline_callback_state_t *state,
    sixel_load_image_function fn_load,
    void *context,
    int header_job_id,
    int decode_job_id)
{
    if (state == NULL) {
        return;
    }
    state->magic = SIXEL_LOADER_TIMELINE_CB_MAGIC;
    state->fn_load = fn_load;
    state->context = context;
    state->header_job_id = header_job_id;
    state->header_closed = 0;
    state->decode_job_id = decode_job_id;
    state->decode_open = decode_job_id >= 0 ? 1 : 0;
}

void
loader_timeline_callback_close_header(
    sixel_loader_timeline_callback_state_t *state,
    SIXELSTATUS status)
{
    if (state == NULL || state->header_closed != 0) {
        return;
    }
    if (state->header_job_id >= 0) {
        loader_timeline_phase_finish("header/read",
                                     state->header_job_id,
                                     status);
    }
    state->header_closed = 1;
}

void
loader_timeline_callback_close_decode(
    sixel_loader_timeline_callback_state_t *state,
    SIXELSTATUS status)
{
    if (state == NULL || state->decode_open == 0) {
        return;
    }
    if (state->decode_job_id >= 0) {
        loader_timeline_phase_finish("decode/pixels",
                                     state->decode_job_id,
                                     status);
    }
    state->decode_open = 0;
    state->decode_job_id = -1;
}

void *
loader_timeline_unwrap_callback_context(void *context)
{
    sixel_loader_timeline_callback_state_t *state;

    state = (sixel_loader_timeline_callback_state_t *)context;
    if (state != NULL && state->magic == SIXEL_LOADER_TIMELINE_CB_MAGIC) {
        return state->context;
    }
    return context;
}

SIXELSTATUS
loader_timeline_emit_frame_callback(sixel_frame_t *frame, void *data)
{
    sixel_loader_timeline_callback_state_t *state;
    SIXELSTATUS status;
    int emit_job_id;
    int next_decode_job_id;

    state = (sixel_loader_timeline_callback_state_t *)data;
    status = SIXEL_BAD_ARGUMENT;
    emit_job_id = -1;
    next_decode_job_id = -1;
    if (state == NULL || state->fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (state->header_closed == 0) {
        loader_timeline_callback_close_header(state, SIXEL_OK);
    }

    loader_timeline_callback_close_decode(state, SIXEL_OK);

    /*
     * The loader timeline must end its decode segment before handing the
     * frame to downstream consumers so callback work is not attributed to
     * loader decode time.
     */
    emit_job_id = loader_timeline_phase_start("emit/frame");
    loader_timeline_phase_finish("emit/frame", emit_job_id, SIXEL_OK);

    status = state->fn_load(frame, state->context);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    next_decode_job_id = loader_timeline_phase_start("decode/pixels");
    state->decode_job_id = next_decode_job_id;
    state->decode_open = next_decode_job_id >= 0 ? 1 : 0;

    return status;
}

int
chunk_is_png(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 8) {
        return 0;
    }

    /*
     * PNG streams begin with an 8-byte signature.  Checking the fixed magic
     * sequence keeps the detection fast and avoids depending on libpng
     * helpers when only the signature is needed.
     */
    if (chunk->buffer[0] == (unsigned char)0x89 &&
        chunk->buffer[1] == 'P' &&
        chunk->buffer[2] == 'N' &&
        chunk->buffer[3] == 'G' &&
        chunk->buffer[4] == (unsigned char)0x0d &&
        chunk->buffer[5] == (unsigned char)0x0a &&
        chunk->buffer[6] == (unsigned char)0x1a &&
        chunk->buffer[7] == (unsigned char)0x0a) {
        return 1;
    }

    return 0;
}

int
chunk_is_jpeg(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 2) {
        return 0;
    }

    /*
     * JPEG files start with SOI (Start of Image) marker 0xFF 0xD8.  The GD
     * loader uses this to decide whether libgd should attempt JPEG decoding.
     */
    if (chunk->buffer[0] == (unsigned char)0xff &&
        chunk->buffer[1] == (unsigned char)0xd8) {
        return 1;
    }

    return 0;
}

int
chunk_is_webp(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 12) {
        return 0;
    }

    /*
     * WebP files use a RIFF container.  The stream starts with \"RIFF\",
     * followed by a 32-bit size field, and then the literal \"WEBP\" tag.
     */
    if (chunk->buffer[0] == 'R' &&
        chunk->buffer[1] == 'I' &&
        chunk->buffer[2] == 'F' &&
        chunk->buffer[3] == 'F' &&
        chunk->buffer[8] == 'W' &&
        chunk->buffer[9] == 'E' &&
        chunk->buffer[10] == 'B' &&
        chunk->buffer[11] == 'P') {
        return 1;
    }

    return 0;
}

int
chunk_is_bmp(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 2) {
        return 0;
    }

    /* BMP headers begin with the literal characters 'B' 'M'. */
    if (chunk->buffer[0] == 'B' && chunk->buffer[1] == 'M') {
        return 1;
    }

    return 0;
}

static int
loader_chunk_read_wbmp_uint(sixel_chunk_t const *chunk,
                            size_t *offset,
                            unsigned int *value)
{
    size_t current;
    unsigned int parsed;
    unsigned char byte;
    int count;

    current = 0u;
    parsed = 0u;
    byte = 0u;
    count = 0;
    if (chunk == NULL || chunk->buffer == NULL ||
            offset == NULL || value == NULL) {
        return 0;
    }

    current = *offset;
    while (count < 5) {
        if (current >= chunk->size) {
            return 0;
        }
        byte = chunk->buffer[current++];
        if (parsed > 0x0fffffffu) {
            return 0;
        }
        parsed = (parsed << 7u) | (unsigned int)(byte & 0x7fu);
        ++count;
        if ((byte & 0x80u) == 0u) {
            *offset = current;
            *value = parsed;
            return 1;
        }
    }

    return 0;
}

int
chunk_is_wbmp(sixel_chunk_t const *chunk)
{
    size_t offset;
    unsigned int type_field;
    unsigned int fixed_header_field;
    unsigned int width;
    unsigned int height;

    offset = 0u;
    type_field = 0u;
    fixed_header_field = 0u;
    width = 0u;
    height = 0u;
    if (chunk == NULL || chunk->buffer == NULL || chunk->size < 4u) {
        return 0;
    }

    /*
     * Level-0 WBMP starts with multi-byte integers:
     *   - TypeField (0),
     *   - FixHeaderField (0),
     *   - width,
     *   - height.
     */
    if (!loader_chunk_read_wbmp_uint(chunk, &offset, &type_field)) {
        return 0;
    }
    if (!loader_chunk_read_wbmp_uint(chunk, &offset, &fixed_header_field)) {
        return 0;
    }
    if (type_field != 0u || fixed_header_field != 0u) {
        return 0;
    }
    if (!loader_chunk_read_wbmp_uint(chunk, &offset, &width)) {
        return 0;
    }
    if (!loader_chunk_read_wbmp_uint(chunk, &offset, &height)) {
        return 0;
    }
    if (width == 0u || height == 0u) {
        return 0;
    }
    if (offset >= chunk->size) {
        return 0;
    }

    return 1;
}

static unsigned short
loader_chunk_read_u16le(unsigned char const *bytes)
{
    if (bytes == NULL) {
        return 0u;
    }

    return (unsigned short)((unsigned short)bytes[0] |
                            (unsigned short)bytes[1] << 8u);
}

int
chunk_is_tga(sixel_chunk_t const *chunk)
{
    unsigned char color_map_type;
    unsigned char image_type;
    unsigned short width;
    unsigned short height;
    unsigned char bits_per_pixel;
    int image_type_supported;

    color_map_type = 0u;
    image_type = 0u;
    width = 0u;
    height = 0u;
    bits_per_pixel = 0u;
    image_type_supported = 0;
    if (chunk == NULL || chunk->buffer == NULL || chunk->size < 18u) {
        return 0;
    }

    color_map_type = chunk->buffer[1];
    image_type = chunk->buffer[2];
    width = loader_chunk_read_u16le(chunk->buffer + 12u);
    height = loader_chunk_read_u16le(chunk->buffer + 14u);
    bits_per_pixel = chunk->buffer[16];
    if (color_map_type > 1u) {
        return 0;
    }

    image_type_supported = image_type == 1u ||
        image_type == 2u ||
        image_type == 3u ||
        image_type == 9u ||
        image_type == 10u ||
        image_type == 11u;
    if (!image_type_supported) {
        return 0;
    }
    if (width == 0u || height == 0u) {
        return 0;
    }
    if (bits_per_pixel != 8u &&
            bits_per_pixel != 15u &&
            bits_per_pixel != 16u &&
            bits_per_pixel != 24u &&
            bits_per_pixel != 32u) {
        return 0;
    }

    return 1;
}

int
chunk_is_tiff(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 4) {
        return 0;
    }

    /*
     * TIFF headers begin with either "II*\0", "MM\0*", or the BigTIFF
     * variants "II+\0"/"MM\0+". Checking the first four bytes is enough to
     * decide whether the stream can be probed by libtiff.
     */
    if ((chunk->buffer[0] == 'I' && chunk->buffer[1] == 'I' &&
         chunk->buffer[2] == (unsigned char)0x2a &&
         chunk->buffer[3] == (unsigned char)0x00) ||
        (chunk->buffer[0] == 'M' && chunk->buffer[1] == 'M' &&
         chunk->buffer[2] == (unsigned char)0x00 &&
         chunk->buffer[3] == (unsigned char)0x2a) ||
        (chunk->buffer[0] == 'I' && chunk->buffer[1] == 'I' &&
         chunk->buffer[2] == (unsigned char)0x2b &&
         chunk->buffer[3] == (unsigned char)0x00) ||
        (chunk->buffer[0] == 'M' && chunk->buffer[1] == 'M' &&
         chunk->buffer[2] == (unsigned char)0x00 &&
         chunk->buffer[3] == (unsigned char)0x2b)) {
        return 1;
    }

    return 0;
}

int
chunk_is_gd2(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->buffer == NULL || chunk->size < 4u) {
        return 0;
    }

    /* GD2 streams start with the literal signature "gd2". */
    if (chunk->buffer[0] == 'g' &&
        chunk->buffer[1] == 'd' &&
        chunk->buffer[2] == '2') {
        return 1;
    }

    return 0;
}

int
chunk_is_gd(sixel_chunk_t const *chunk)
{
    unsigned short width;
    unsigned short height;
    unsigned short ncolors;

    width = 0u;
    height = 0u;
    ncolors = 0u;
    if (chunk == NULL || chunk->buffer == NULL || chunk->size < 6u) {
        return 0;
    }
    if (chunk_is_gd2(chunk)) {
        return 0;
    }

    /*
     * Legacy GD files have no fixed magic. Use conservative header sanity
     * checks to avoid probing unrelated formats.
     */
    width = (unsigned short)((unsigned short)chunk->buffer[0] << 8u |
                             (unsigned short)chunk->buffer[1]);
    height = (unsigned short)((unsigned short)chunk->buffer[2] << 8u |
                              (unsigned short)chunk->buffer[3]);
    ncolors = (unsigned short)((unsigned short)chunk->buffer[4] << 8u |
                               (unsigned short)chunk->buffer[5]);
    if (width == 0u || height == 0u) {
        return 0;
    }
    if (ncolors == 0u || ncolors > 256u) {
        return 0;
    }

    return 1;
}

int
chunk_is_gif(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->buffer == NULL || chunk->size < 6u) {
        return 0;
    }
    if (chunk->buffer[0] == 'G' &&
        chunk->buffer[1] == 'I' &&
        chunk->buffer[2] == 'F' &&
        chunk->buffer[3] == '8' &&
        (chunk->buffer[4] == '7' || chunk->buffer[4] == '9') &&
        chunk->buffer[5] == 'a') {
        return 1;
    }
    return 0;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
