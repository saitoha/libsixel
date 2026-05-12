/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2026 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
#include <stdarg.h>

# if HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_UNISTD_H
# include <unistd.h>
#elif HAVE_SYS_UNISTD_H
# include <sys/unistd.h>
#endif  /* HAVE_SYS_UNISTD_H */
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && HAVE_SIGNAL_H
# include <signal.h>
#endif  /* !_WIN32 && !__EMSCRIPTEN__ && HAVE_SIGNAL_H */
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif  /* HAVE_INTTYPES_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif  /* HAVE_SYS_STAT_H */
#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif  /* !S_ISDIR */
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#elif HAVE_TIME_H
# include <time.h>
#endif  /* HAVE_SYS_TIME_H HAVE_TIME_H */
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif  /* HAVE_SYS_IOCTL_H */
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif  /* HAVE_FCNTL_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_CTYPE_H
# include <ctype.h>
#endif  /* HAVE_CTYPE_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */

#include <sixel.h>
#include "loader.h"
#include "loader-common.h"
#include "loader-order-schema.h"
#include "tty.h"
#include "encoder.h"
#include "frame.h"
#include "frame-factory.h"
#include "encoder-core.h"
#include "output-factory.h"
#include "timeline-logger.h"
#include "options.h"
#include "dither.h"
#include "dither-interframe-method.h"
#include "palette-kcenter.h"
#include "palette-kmeans.h"
#include "palette-kmedoids.h"
#include "palette-common-merge.h"
#include "pixelformat.h"
#include "clipboard.h"
#include "compat_stub.h"
#include "path.h"
#include "mapfile.h"
#include "drcs.h"
#include "filter.h"
#include "filter-clip.h"
#include "filter-colors.h"
#include "filter-dither.h"
#include "filter-gradient.h"
#include "filter-final-merge.h"
#include "filter-factory.h"
#include "filter-palette.h"
#include "filter-fhedt.h"
#include "filter-vptree.h"
#include "filter-eytzinger.h"
#include "filter-resize.h"
#include "filter-sample.h"
#include "sleep.h"
#include "threading.h"
#include "planner.h"
#include "sixel_atomic.h"

#define SIXEL_ENCODER_PRECISION_ENVVAR "SIXEL_FLOAT32_DITHER"
#define SIXEL_ENCODER_LUT_POLICY_ENVVAR "SIXEL_DITHER_LOOKUP_POLICY"
#define SIXEL_ENCODER_SAMPLE_TARGET_ENVVAR \
    "SIXEL_PALETTE_SAMPLE_TARGET"
#define SIXEL_PALETTE_ANIMATION_MODE_ENVVAR \
    "SIXEL_PALETTE_ANIMATION_MODE"
#define SIXEL_PALETTE_SCENE_CUT_THRESHOLD_ENVVAR \
    "SIXEL_PALETTE_SCENE_CUT_THRESHOLD"
#define SIXEL_ENCODER_ANIMATION_HIDE_CURSOR_ENVVAR \
    "SIXEL_ANIMATION_HIDE_CURSOR"
#define SIXEL_ENCODER_PSD_TRACE_ONLY_ENVVAR \
    "SIXEL_PSD_TRACE_ONLY"
#define SIXEL_ENCODER_SIGINT_SYNC_PID_ENVVAR \
    "LSO_TEST_SIGINT_NOTIFY_PID"
#define SIXEL_ENCODER_SIGINT_SYNC_EVENT_ENVVAR \
    "LSO_TEST_SIGINT_NOTIFY_EVENT"
#define SIXEL_ENCODER_SIGINT_SYNC_FD_ENVVAR \
    "LSO_TEST_SIGINT_NOTIFY_FD"
#define SIXEL_ENCODER_SIGINT_SYNC_EVENT_DISPATCH_START \
    "callback_dispatch_start"

#if defined(__EMSCRIPTEN__) && HAVE_MKSTEMP
# define SIXEL_ENCODER_USE_MKSTEMP_PNG_STAGING 1
#else
# define SIXEL_ENCODER_USE_MKSTEMP_PNG_STAGING 0
#endif

#define SIXEL_QUANTIZE_SCENE_CUT_THRESHOLD_DEFAULT 0.20
#define SIXEL_QUANTIZE_SCENE_PROBE_GRID_SIDE 8
#define SIXEL_QUANTIZE_SCENE_PROBE_COUNT \
    (SIXEL_QUANTIZE_SCENE_PROBE_GRID_SIDE \
     * SIXEL_QUANTIZE_SCENE_PROBE_GRID_SIDE)
#define SIXEL_QUANTIZE_SCENE_PROBE_BYTES \
    (SIXEL_QUANTIZE_SCENE_PROBE_COUNT * 3)

#if defined(_MSC_VER)
# define SIXEL_ENCODER_OVERRIDE_TLS_AVAILABLE 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !defined(__PCC__)
# define SIXEL_ENCODER_OVERRIDE_TLS_AVAILABLE 1
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__PCC__)
# define SIXEL_ENCODER_OVERRIDE_TLS_AVAILABLE 1
#else
# define SIXEL_ENCODER_OVERRIDE_TLS_AVAILABLE 0
#endif

#if SIXEL_ENABLE_THREADS && !SIXEL_ENCODER_OVERRIDE_TLS_AVAILABLE
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
static CRITICAL_SECTION sixel_encoder_quantize_override_mutex;
static INIT_ONCE sixel_encoder_quantize_override_once
    = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
sixel_encoder_quantize_override_lock_init_once(PINIT_ONCE once,
                                               PVOID parameter,
                                               PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;

    InitializeCriticalSection(&sixel_encoder_quantize_override_mutex);
    return TRUE;
}
# else
#  include <pthread.h>
/*
 * Use runtime initialization so Cosmopolitan/pcc builds avoid warnings from
 * partial pthread struct initializers.
 */
static pthread_mutex_t sixel_encoder_quantize_override_mutex;
static pthread_once_t sixel_encoder_quantize_override_mutex_once
    = PTHREAD_ONCE_INIT;
static int sixel_encoder_quantize_override_mutex_ready = 0;

static void
sixel_encoder_quantize_override_lock_init_once(void)
{
    int rc;

    rc = pthread_mutex_init(&sixel_encoder_quantize_override_mutex, NULL);
    if (rc == 0) {
        sixel_encoder_quantize_override_mutex_ready = 1;
    }
}
# endif

/*
 * Quantize overrides are stored in TLS variables inside the palette backends.
 * Compilers without TLS support collapse those into process globals, so
 * concurrent encodes would otherwise race while one thread updates temporary
 * overrides for another thread's palette build.
 */
static int
sixel_encoder_quantize_override_lock_acquire(void)
{
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    BOOL initialized;

    initialized = InitOnceExecuteOnce(
        &sixel_encoder_quantize_override_once,
        sixel_encoder_quantize_override_lock_init_once,
        NULL,
        NULL);
    if (!initialized) {
        return 0;
    }
    EnterCriticalSection(&sixel_encoder_quantize_override_mutex);
    return 1;
# else
    int rc;
    int once_status;

    once_status = pthread_once(&sixel_encoder_quantize_override_mutex_once,
                               sixel_encoder_quantize_override_lock_init_once);
    if (once_status != 0 || !sixel_encoder_quantize_override_mutex_ready) {
        return 0;
    }

    rc = pthread_mutex_lock(&sixel_encoder_quantize_override_mutex);
    if (rc != 0) {
        return 0;
    }
    return 1;
# endif
}

static void
sixel_encoder_quantize_override_lock_release(int acquired)
{
    if (acquired == 0) {
        return;
    }
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    LeaveCriticalSection(&sixel_encoder_quantize_override_mutex);
# else
    if (!sixel_encoder_quantize_override_mutex_ready) {
        return;
    }
    (void)pthread_mutex_unlock(&sixel_encoder_quantize_override_mutex);
# endif
}
#else
static int
sixel_encoder_quantize_override_lock_acquire(void)
{
    return 0;
}

static void
sixel_encoder_quantize_override_lock_release(int acquired)
{
    (void)acquired;
}
#endif

typedef enum sixel_encoder_precision_mode {
    SIXEL_ENCODER_PRECISION_MODE_AUTO = 0,
    SIXEL_ENCODER_PRECISION_MODE_8BIT,
    SIXEL_ENCODER_PRECISION_MODE_FLOAT32
} sixel_encoder_precision_mode_t;

static void clipboard_select_format(char *dest,
                                    size_t dest_size,
                                    char const *format,
                                    char const *fallback);
static SIXELSTATUS clipboard_create_spool(sixel_allocator_t *allocator,
                                          char const *prefix,
                                          char **path_out,
                                          int *fd_out);
static SIXELSTATUS clipboard_write_file(char const *path,
                                        unsigned char const *data,
                                        size_t size);
static SIXELSTATUS clipboard_read_file(char const *path,
                                       unsigned char **data,
                                       size_t *size);
static int sixel_encoder_threads_token_is_auto(char const *text);
static int sixel_encoder_parse_threads_argument(char const *text,
                                                int *value);
static SIXELSTATUS sixel_encoder_apply_lut_filter(sixel_encoder_t *encoder,
                                                  sixel_dither_t *dither);
static int sixel_encoder_pixelformat_has_alpha(int pixelformat);
static int sixel_encoder_frame_has_transparent_mask(
    sixel_frame_t const *frame);
static int sixel_encoder_frame_preserves_alpha_key(
    sixel_frame_t const *frame);
static int sixel_encoder_frame_get_transparent_mask_pixels(
    sixel_frame_t const *frame,
    size_t *pixel_count_out);
static void sixel_encoder_bind_frame_transparent_mask(
    sixel_dither_t *dither,
    sixel_frame_t const *frame);
static SIXELSTATUS sixel_encoder_promote_pal8_transparent_for_geometry(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame);

#define SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY 4
#define SIXEL_TRACE_TOPIC_ENCODE_HANDOFF "encode_handoff"

typedef enum sixel_encoder_handoff_mode {
    SIXEL_ENCODER_HANDOFF_UNDECIDED = 0,
    SIXEL_ENCODER_HANDOFF_SERIAL = 1,
    SIXEL_ENCODER_HANDOFF_PIPELINE = 2
} sixel_encoder_handoff_mode_t;

typedef enum sixel_encoder_handoff_trace_reason {
    SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE = 0,
    SIXEL_ENCODER_HANDOFF_TRACE_REASON_CALLBACK_CANCEL,
    SIXEL_ENCODER_HANDOFF_TRACE_REASON_PLANNER_DISALLOW,
    SIXEL_ENCODER_HANDOFF_TRACE_REASON_THREAD_CREATE_FAILED,
    SIXEL_ENCODER_HANDOFF_TRACE_REASON_ENQUEUE_CANCEL_BEFORE_LOCK,
    SIXEL_ENCODER_HANDOFF_TRACE_REASON_ENQUEUE_CANCEL_PREWAIT,
    SIXEL_ENCODER_HANDOFF_TRACE_REASON_ENQUEUE_CANCEL_WAIT,
    SIXEL_ENCODER_HANDOFF_TRACE_REASON_WORKER_CLONE_FAILED,
    SIXEL_ENCODER_HANDOFF_TRACE_REASON_WORKER_ERROR,
    SIXEL_ENCODER_HANDOFF_TRACE_REASON_FINISH_CANCEL
} sixel_encoder_handoff_trace_reason_t;

typedef enum sixel_encoder_handoff_trace_event {
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_ENTER = 0,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_PLANNER,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_HANDOFF_DECIDE,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_DISPATCH_START,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_ENQUEUE_REQUEST,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_ENQUEUE_RESULT,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_SERIAL_START,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_SERIAL_RESULT,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_PIPELINE_STOP,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_BY_REF,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_CLONE_FAILED,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_WAIT,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_ABORT,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_OK,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_START,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_WAIT,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_BREAK,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_POP,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_CLONE_FALLBACK_ENABLED,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_ENCODED,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_EXIT,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_FINISH_BEGIN,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_FINISH_JOIN_WAIT,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_FINISH_JOIN_DONE,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_FINISH_END,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENCODE_LOADER_RESULT,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENCODE_PIPELINE_FINISH_BEGIN,
    SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENCODE_PIPELINE_FINISH_END
} sixel_encoder_handoff_trace_event_t;

typedef struct sixel_palette_async_job {
    sixel_thread_t thread;
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    sixel_encoder_t *encoder;
    sixel_timeline_logger_t *logger;
    sixel_frame_t *sample_frame;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    SIXELSTATUS status;
    int target_pixelformat;
    int reqcolors;
    int method_for_largest;
    int method_for_rep;
    int quality_mode;
    int lut_policy;
    int final_merge_mode;
    int sixel_reversible;
    int quantize_model;
    int force_palette;
    int frame_no;
    int loop_no;
    int multiframe;
    int started;
    int finished;
} sixel_palette_async_job_t;

typedef struct sixel_palette_builder_context {
    sixel_encoder_t *encoder;
    int allow_cache;
} sixel_palette_builder_context_t;

typedef struct sixel_encoder_frame_handoff_meta {
    int frame_no;
    int loop_no;
    int delay;
    int multiframe;
    int needs_preplan_clone;
} sixel_encoder_frame_handoff_meta_t;

typedef struct sixel_encoder_frame_pipeline {
    sixel_thread_t thread;
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    sixel_frame_t *queue[SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY];
    sixel_encoder_frame_handoff_meta_t
        queue_meta[SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY];
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    SIXELSTATUS worker_status;
    int queue_head;
    int queue_tail;
    int queue_count;
    int initialized;
    int started;
    int loader_done;
    sixel_encoder_handoff_mode_t handoff_mode;
} sixel_encoder_frame_pipeline_t;

typedef struct sixel_encoder_load_context {
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_encoding_planner_t *planner;
    sixel_allocator_t *allocator;
    int psd_trace_only;
    sixel_encoder_frame_pipeline_t frame_pipeline;
} sixel_encoder_load_context_t;

static int
sixel_encoder_path_has_extension(char const *path, char const *extension)
{
    char const *basename;
    char const *dot;

    basename = NULL;
    dot = NULL;
    if (path == NULL || extension == NULL || extension[0] == '\0') {
        return 0;
    }

    basename = strrchr(path, '/');
#if defined(_WIN32)
    {
        char const *backslash;

        backslash = strrchr(path, '\\');
        if (backslash != NULL && (basename == NULL || backslash > basename)) {
            basename = backslash;
        }
    }
#endif
    if (basename == NULL) {
        basename = path;
    } else {
        ++basename;
    }
    if (basename[0] == '\0') {
        return 0;
    }

    dot = strrchr(basename, '.');
    if (dot == NULL) {
        return 0;
    }

    return sixel_compat_strcasecmp(dot + 1, extension) == 0;
}

static int
sixel_encoder_should_force_auto_lut_for_psd(
    sixel_encoder_t const *encoder,
    char const *path)
{
    if (encoder == NULL || path == NULL) {
        return 0;
    }
    if (encoder->lut_policy != SIXEL_LUT_POLICY_CERTLUT) {
        return 0;
    }
    return sixel_encoder_path_has_extension(path, "psd") ||
           sixel_encoder_path_has_extension(path, "psb");
}

static int
sixel_encoder_envvar_is_enabled(char const *envvar)
{
    char const *value;

    value = NULL;
    if (envvar == NULL) {
        return 0;
    }

    value = sixel_compat_getenv(envvar);
    if (value == NULL) {
        return 0;
    }
    if (value[0] == '1' && value[1] == '\0') {
        return 1;
    }

    return 0;
}

static int
sixel_encoder_should_use_psd_trace_only(char const *path)
{
    if (path == NULL) {
        return 0;
    }
    if (!sixel_encoder_envvar_is_enabled(
            SIXEL_ENCODER_PSD_TRACE_ONLY_ENVVAR)) {
        return 0;
    }
    if (!sixel_trace_topic_is_enabled("psd_decode")) {
        return 0;
    }
    return sixel_encoder_path_has_extension(path, "psd") ||
           sixel_encoder_path_has_extension(path, "psb");
}

static void
sixel_encoder_emit_contract_code(FILE *stream,
                                 int *first,
                                 char const *code)
{
    if (stream == NULL || first == NULL || code == NULL || code[0] == '\0') {
        return;
    }
    if (*first == 0) {
        fprintf(stream, ",");
    }
    fprintf(stream, "%s", code);
    *first = 0;
}

static void
sixel_encoder_update_diagnostic_dither(sixel_encoder_t *encoder,
                                       sixel_dither_t *dither)
{
    if (encoder == NULL) {
        return;
    }

    if (encoder->diagnostic_dither != NULL) {
        sixel_dither_unref((sixel_dither_t *)encoder->diagnostic_dither);
        encoder->diagnostic_dither = NULL;
    }
    if (dither != NULL) {
        encoder->diagnostic_dither = dither;
        sixel_dither_ref(dither);
    }
}

static char const *
sixel_encoder_dither_source_name(unsigned int source_id)
{
    if (source_id == SIXEL_INTERFRAME_STBN_SOURCE_MASK) {
        return "mask";
    }
    if (source_id == SIXEL_INTERFRAME_STBN_SOURCE_PMJ) {
        return "pmj";
    }

    return "hash";
}

static void
sixel_encoder_emit_dither_contract(sixel_encoder_t const *encoder,
                                   SIXELSTATUS status)
{
    sixel_dither_t const *dither;
    unsigned int source_id;
    int first;

    dither = NULL;
    source_id = SIXEL_INTERFRAME_STBN_SOURCE_HASH;
    first = 1;
    if (encoder == NULL) {
        return;
    }
    if (!sixel_trace_topic_is_enabled("dither_contract")) {
        return;
    }

    dither = (sixel_dither_t const *)encoder->dither_cache;
    if (dither == NULL) {
        dither = (sixel_dither_t const *)encoder->diagnostic_dither;
    }
    if (dither == NULL) {
        return;
    }
    source_id = sixel_interframe_stbn_source_id_from_token(
        dither->interframe_strategy_token);

    fprintf(stderr,
            "LSXDTH1|rc=%d|apply=%lu|consume=%lu|reset=%lu|"
            "reset_boundary=%lu|reset_size=%lu|source=%s|codes=",
            status,
            dither->interframe_state.apply_count,
            dither->interframe_state.consume_count,
            dither->interframe_state.reset_count,
            dither->interframe_state.reset_frame_boundary_count,
            dither->interframe_state.reset_size_change_count,
            sixel_encoder_dither_source_name(source_id));
    if (dither->method_for_diffuse == SIXEL_DIFFUSE_INTERFRAME) {
        sixel_encoder_emit_contract_code(stderr, &first, "INTERFRAME_ENABLED");
    }
    if (source_id == SIXEL_INTERFRAME_STBN_SOURCE_PMJ) {
        sixel_encoder_emit_contract_code(stderr, &first, "STRATEGY_SOURCE_PMJ");
    }
    if (dither->stbn_alpha_guard_override != 0
            && dither->stbn_alpha_guard_enabled != 0) {
        sixel_encoder_emit_contract_code(stderr, &first, "ALPHA_GUARD_ON");
    } else if (dither->stbn_alpha_guard_override != 0) {
        sixel_encoder_emit_contract_code(stderr, &first, "ALPHA_GUARD_OFF");
    }
    if (dither->interframe_state.reset_frame_boundary_count > 0UL) {
        sixel_encoder_emit_contract_code(stderr,
                                         &first,
                                         "RESET_BETWEEN_INPUTS");
    }
    if (dither->interframe_state.reset_size_change_count > 0UL) {
        sixel_encoder_emit_contract_code(stderr,
                                         &first,
                                         "RESET_ON_SIZE_CHANGE");
    }
    if (first != 0) {
        fprintf(stderr, "DTH_OK");
    }
    fprintf(stderr, "\n");
}

static char const *
sixel_encoder_palette_model_name(int quantize_model)
{
    if (quantize_model == SIXEL_QUANTIZE_MODEL_MEDIANCUT) {
        return "heckbert";
    }
    if (quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
        return "kmeans";
    }
    if (quantize_model == SIXEL_QUANTIZE_MODEL_KMEDOIDS) {
        return "medoids";
    }
    if (quantize_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
        return "center";
    }

    return "auto";
}

enum {
    SIXEL_HECKBERT_PROFILE_COMPAT = 0,
    SIXEL_HECKBERT_PROFILE_SPEED = 1,
    SIXEL_HECKBERT_PROFILE_QUALITY = 2
};

static char const *
sixel_encoder_palette_merge_name(int merge_mode)
{
    if (merge_mode == SIXEL_FINAL_MERGE_NONE) {
        return "none";
    }
    if (merge_mode == SIXEL_FINAL_MERGE_WARD) {
        return "ward";
    }

    return "auto";
}

static char const *
sixel_encoder_palette_lut_name(int lut_policy)
{
    if (lut_policy == SIXEL_LUT_POLICY_5BIT) {
        return "5bit";
    }
    if (lut_policy == SIXEL_LUT_POLICY_6BIT) {
        return "6bit";
    }
    if (lut_policy == SIXEL_LUT_POLICY_NONE) {
        return "none";
    }
    if (lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        return "certlut";
    }
    if (lut_policy == SIXEL_LUT_POLICY_FHEDT) {
        return "fhedt";
    }
    if (lut_policy == SIXEL_LUT_POLICY_EYTZINGER) {
        return "eytzinger";
    }
    if (lut_policy == SIXEL_LUT_POLICY_VPTREE) {
        return "vptree";
    }
    if (lut_policy == SIXEL_LUT_POLICY_RBC) {
        return "rbc";
    }
    if (lut_policy == SIXEL_LUT_POLICY_MAHALANOBIS) {
        return "mahalanobis";
    }

    return "auto";
}

static double
sixel_encoder_heckbert_quality_oversplit_factor(int reqcolors)
{
    unsigned int count;

    count = reqcolors > 0 ? (unsigned int)reqcolors : 256U;
    if (count <= 32U) {
        return 2.0;
    }
    if (count <= 64U) {
        return 1.8;
    }
    if (count <= 128U) {
        return 1.6;
    }

    return 1.4;
}

/*
 * Quality profile uses a larger oversplit budget for small palettes so Ward
 * merge can recover local color islands.  Large palettes keep the factor
 * closer to 1.0 to cap merge cost.
 */
static void
sixel_encoder_apply_heckbert_profile_defaults(sixel_encoder_t *encoder)
{
    int profile;

    if (encoder == NULL) {
        return;
    }
    if (encoder->quantize_model != SIXEL_QUANTIZE_MODEL_MEDIANCUT) {
        return;
    }

    /*
     * Explicit suboptions still win: only fill fields whose override flag is
     * not set so `-Q ...:profile=...:merge=...` keeps the explicit merge
     * choice, and `-f` keeps the explicit split-axis choice.
     */
    profile = encoder->quantize_model_heckbert_profile;
    switch (profile) {
    case SIXEL_HECKBERT_PROFILE_SPEED:
        if (encoder->quantize_model_merge_override == 0) {
            encoder->quantize_model_merge_mode = SIXEL_FINAL_MERGE_NONE;
            encoder->final_merge_mode = SIXEL_FINAL_MERGE_NONE;
        }
        if (encoder->quantize_model_merge_lloyd_override == 0) {
            encoder->quantize_model_merge_lloyd = 3u;
        }
        if (encoder->method_for_largest_override == 0) {
            encoder->method_for_largest = SIXEL_LARGE_NORM;
        }
        break;
    case SIXEL_HECKBERT_PROFILE_QUALITY:
        if (encoder->quantize_model_merge_override == 0) {
            encoder->quantize_model_merge_mode = SIXEL_FINAL_MERGE_WARD;
            encoder->final_merge_mode = SIXEL_FINAL_MERGE_WARD;
        }
        if (encoder->quantize_model_merge_lloyd_override == 0) {
            encoder->quantize_model_merge_lloyd = 2u;
        }
        if (encoder->method_for_largest_override == 0) {
            encoder->method_for_largest = SIXEL_LARGE_PCA;
        }
        break;
    case SIXEL_HECKBERT_PROFILE_COMPAT:
    default:
        if (encoder->quantize_model_merge_override == 0) {
            encoder->quantize_model_merge_mode = SIXEL_FINAL_MERGE_AUTO;
            encoder->final_merge_mode = SIXEL_FINAL_MERGE_AUTO;
        }
        if (encoder->quantize_model_merge_lloyd_override == 0) {
            encoder->quantize_model_merge_lloyd = 3u;
        }
        if (encoder->method_for_largest_override == 0) {
            encoder->method_for_largest = SIXEL_LARGE_AUTO;
        }
        break;
    }
}

static void
sixel_encoder_emit_palette_contract(sixel_encoder_t const *encoder,
                                    SIXELSTATUS status)
{
    int first;
    int kmeans_seed_set;
    unsigned int kmeans_seed;

    first = 1;
    kmeans_seed_set = 0;
    kmeans_seed = 0u;
    if (encoder == NULL) {
        return;
    }
    if (!sixel_trace_topic_is_enabled("palette_contract")) {
        return;
    }
    if (encoder->quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS
            && encoder->quantize_model_kmeans_seed_override != 0) {
        kmeans_seed_set = 1;
        kmeans_seed = encoder->quantize_model_kmeans_seed;
    }

    fprintf(stderr,
            "LSXPAL1|rc=%d|model=%s|merge=%s|lut=%s|work=%d|cluster=%d|"
            "kseed_set=%d|kseed=%u|codes=",
            status,
            sixel_encoder_palette_model_name(encoder->quantize_model),
            sixel_encoder_palette_merge_name(
                encoder->quantize_model_merge_mode),
            sixel_encoder_palette_lut_name(encoder->lut_policy),
            encoder->working_colorspace,
            encoder->clustering_colorspace,
            kmeans_seed_set,
            kmeans_seed);
    if (encoder->quantize_model == SIXEL_QUANTIZE_MODEL_MEDIANCUT) {
        sixel_encoder_emit_contract_code(stderr, &first, "MODEL_HECKBERT");
    } else if (encoder->quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
        sixel_encoder_emit_contract_code(stderr, &first, "MODEL_KMEANS");
    } else if (encoder->quantize_model == SIXEL_QUANTIZE_MODEL_KMEDOIDS) {
        sixel_encoder_emit_contract_code(stderr, &first, "MODEL_MEDOIDS");
    } else if (encoder->quantize_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
        sixel_encoder_emit_contract_code(stderr, &first, "MODEL_CENTER");
    } else {
        sixel_encoder_emit_contract_code(stderr, &first, "MODEL_AUTO");
    }
    if (encoder->quantize_model_merge_mode == SIXEL_FINAL_MERGE_WARD) {
        sixel_encoder_emit_contract_code(stderr, &first, "MERGE_WARD");
    } else if (encoder->quantize_model_merge_mode == SIXEL_FINAL_MERGE_NONE) {
        sixel_encoder_emit_contract_code(stderr, &first, "MERGE_NONE");
    } else {
        sixel_encoder_emit_contract_code(stderr, &first, "MERGE_AUTO");
    }
    if (encoder->lut_policy == SIXEL_LUT_POLICY_FHEDT) {
        sixel_encoder_emit_contract_code(stderr, &first, "LUT_FHEDT");
    } else if (encoder->lut_policy == SIXEL_LUT_POLICY_VPTREE) {
        sixel_encoder_emit_contract_code(stderr, &first, "LUT_VPTREE");
    } else if (encoder->lut_policy == SIXEL_LUT_POLICY_EYTZINGER) {
        sixel_encoder_emit_contract_code(stderr, &first, "LUT_EYTZINGER");
    } else if (encoder->lut_policy == SIXEL_LUT_POLICY_NONE) {
        sixel_encoder_emit_contract_code(stderr, &first, "LUT_NONE");
    } else if (encoder->lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        sixel_encoder_emit_contract_code(stderr, &first, "LUT_CERTLUT");
    } else if (encoder->lut_policy == SIXEL_LUT_POLICY_5BIT) {
        sixel_encoder_emit_contract_code(stderr, &first, "LUT_5BIT");
    } else if (encoder->lut_policy == SIXEL_LUT_POLICY_6BIT) {
        sixel_encoder_emit_contract_code(stderr, &first, "LUT_6BIT");
    } else {
        sixel_encoder_emit_contract_code(stderr, &first, "LUT_AUTO");
    }
    if (encoder->working_colorspace_set != 0) {
        sixel_encoder_emit_contract_code(stderr, &first, "WORKING_SET");
    }
    if (encoder->clustering_colorspace_set != 0) {
        sixel_encoder_emit_contract_code(stderr, &first, "CLUSTER_SET");
    }
    if (kmeans_seed_set != 0) {
        sixel_encoder_emit_contract_code(stderr, &first, "KMEANS_SEED_SET");
    }
    if (first != 0) {
        fprintf(stderr, "PAL_OK");
    }
    fprintf(stderr, "\n");
}

static char const *
sixel_encoder_handoff_mode_name(sixel_encoder_handoff_mode_t mode)
{
    switch (mode) {
    case SIXEL_ENCODER_HANDOFF_UNDECIDED:
        return "undecided";
    case SIXEL_ENCODER_HANDOFF_SERIAL:
        return "serial";
    case SIXEL_ENCODER_HANDOFF_PIPELINE:
        return "pipeline";
    default:
        return "unknown";
    }
}

static char const *
sixel_encoder_handoff_trace_reason_name(
    sixel_encoder_handoff_trace_reason_t reason)
{
    switch (reason) {
    case SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE:
        return "none";
    case SIXEL_ENCODER_HANDOFF_TRACE_REASON_CALLBACK_CANCEL:
        return "callback_cancel";
    case SIXEL_ENCODER_HANDOFF_TRACE_REASON_PLANNER_DISALLOW:
        return "planner_disallow";
    case SIXEL_ENCODER_HANDOFF_TRACE_REASON_THREAD_CREATE_FAILED:
        return "thread_create_failed";
    case SIXEL_ENCODER_HANDOFF_TRACE_REASON_ENQUEUE_CANCEL_BEFORE_LOCK:
        return "enqueue_cancel_before_lock";
    case SIXEL_ENCODER_HANDOFF_TRACE_REASON_ENQUEUE_CANCEL_PREWAIT:
        return "enqueue_cancel_prewait";
    case SIXEL_ENCODER_HANDOFF_TRACE_REASON_ENQUEUE_CANCEL_WAIT:
        return "enqueue_cancel_wait";
    case SIXEL_ENCODER_HANDOFF_TRACE_REASON_WORKER_CLONE_FAILED:
        return "worker_clone_failed";
    case SIXEL_ENCODER_HANDOFF_TRACE_REASON_WORKER_ERROR:
        return "worker_error";
    case SIXEL_ENCODER_HANDOFF_TRACE_REASON_FINISH_CANCEL:
        return "finish_cancel";
    default:
        return "unknown";
    }
}

static char const *
sixel_encoder_handoff_trace_event_name(
    sixel_encoder_handoff_trace_event_t event)
{
    switch (event) {
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_ENTER:
        return "callback_enter";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_PLANNER:
        return "callback_planner";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_HANDOFF_DECIDE:
        return "callback_handoff_decide";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_DISPATCH_START:
        return "callback_dispatch_start";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_ENQUEUE_REQUEST:
        return "callback_enqueue_request";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_ENQUEUE_RESULT:
        return "callback_enqueue_result";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_SERIAL_START:
        return "callback_serial_start";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_SERIAL_RESULT:
        return "callback_serial_result";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_PIPELINE_STOP:
        return "pipeline_stop";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_BY_REF:
        return "enqueue_by_ref";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_CLONE_FAILED:
        return "enqueue_clone_failed";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_WAIT:
        return "enqueue_wait";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_ABORT:
        return "enqueue_abort";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_OK:
        return "enqueue_ok";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_START:
        return "worker_start";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_WAIT:
        return "worker_wait";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_BREAK:
        return "worker_break";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_POP:
        return "worker_pop";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_CLONE_FALLBACK_ENABLED:
        return "worker_clone_fallback_enabled";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_ENCODED:
        return "worker_encoded";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_EXIT:
        return "worker_exit";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_FINISH_BEGIN:
        return "finish_begin";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_FINISH_JOIN_WAIT:
        return "finish_join_wait";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_FINISH_JOIN_DONE:
        return "finish_join_done";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_FINISH_END:
        return "finish_end";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENCODE_LOADER_RESULT:
        return "encode_loader_result";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENCODE_PIPELINE_FINISH_BEGIN:
        return "encode_pipeline_finish_begin";
    case SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENCODE_PIPELINE_FINISH_END:
        return "encode_pipeline_finish_end";
    default:
        return "unknown";
    }
}

static int
sixel_encoder_handoff_trace_minimal_enabled(void)
{
    char const *value;
    int enabled;

    value = NULL;
    enabled = 0;

    value = sixel_compat_getenv("SIXEL_ENCODE_HANDOFF_TRACE_MINIMAL");
    if (value != NULL && strcmp(value, "1") == 0) {
        enabled = 1;
    }

    return enabled;
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && HAVE_SIGNAL_H
static void
sixel_encoder_test_sigint_sync_notify_dispatch_start(void)
{
    static int sync_state = -1;
    static pid_t sync_pid = (pid_t)0;
    static int sync_fd = -1;
    char const *sync_event;
    char const *sync_pid_text;
    char const *sync_fd_text;
    unsigned long parsed_pid;
    unsigned long parsed_fd;
    char *sync_end;
    char *sync_fd_end;
    int notify_result;
    ssize_t fd_write_result;
    char notify_token;

    sync_event = NULL;
    sync_pid_text = NULL;
    sync_fd_text = NULL;
    parsed_pid = 0ul;
    parsed_fd = 0ul;
    sync_end = NULL;
    sync_fd_end = NULL;
    notify_result = -1;
    fd_write_result = (ssize_t)-1;
    notify_token = '1';

    sync_event = sixel_compat_getenv(
        SIXEL_ENCODER_SIGINT_SYNC_EVENT_ENVVAR);
    sync_fd_text = sixel_compat_getenv(
        SIXEL_ENCODER_SIGINT_SYNC_FD_ENVVAR);
    sync_pid_text = sixel_compat_getenv(
        SIXEL_ENCODER_SIGINT_SYNC_PID_ENVVAR);

    if ((sync_event == NULL || sync_event[0] == '\0') &&
            (sync_fd_text == NULL || sync_fd_text[0] == '\0') &&
            (sync_pid_text == NULL || sync_pid_text[0] == '\0')) {
        return;
    }
    if (sync_event != NULL
            && sync_event[0] != '\0'
            && strcmp(sync_event,
                      SIXEL_ENCODER_SIGINT_SYNC_EVENT_DISPATCH_START)
               != 0) {
        return;
    }
    if (sync_state == 0 || sync_state == 2) {
        return;
    }

    if (sync_state < 0) {
        sync_state = 0;
        if (sync_fd_text != NULL && sync_fd_text[0] != '\0') {
            errno = 0;
            parsed_fd = strtoul(sync_fd_text, &sync_fd_end, 10);
            if (errno == 0
                    && sync_fd_end != sync_fd_text
                    && *sync_fd_end == '\0'
                    && parsed_fd > 0ul
                    && parsed_fd <= (unsigned long)INT_MAX) {
                sync_fd = (int)parsed_fd;
            }
        }

        if (sync_pid_text != NULL && sync_pid_text[0] != '\0') {
            errno = 0;
            parsed_pid = strtoul(sync_pid_text, &sync_end, 10);
            if (errno == 0
                    && sync_end != sync_pid_text
                    && *sync_end == '\0'
                    && parsed_pid != 0ul
                    && parsed_pid <= (unsigned long)LONG_MAX) {
                sync_pid = (pid_t)parsed_pid;
            }
        }

        if (sync_fd >= 0 || sync_pid > (pid_t)0) {
            sync_state = 1;
        }
    }

    if (sync_fd < 0 && sync_pid <= (pid_t)0) {
        return;
    }

    if (sync_fd >= 0) {
        do {
            fd_write_result = write(sync_fd, &notify_token, 1u);
        } while (fd_write_result < (ssize_t)0 && errno == EINTR);
        if (fd_write_result == (ssize_t)1 || errno != EINTR) {
            (void)close(sync_fd);
            sync_fd = -1;
        }
    }

    if (sync_pid > (pid_t)0) {
        notify_result = kill(sync_pid, SIGUSR1);
        if (notify_result == 0 || errno != EINTR) {
            sync_pid = (pid_t)0;
        }
    }

    if (sync_fd < 0 && sync_pid <= (pid_t)0) {
        /*
         * Emit only once per process and keep retries only for EINTR.
         */
        sync_state = 2;
    }
}
#else
static void
sixel_encoder_test_sigint_sync_notify_dispatch_start(void)
{
}
#endif

static int
sixel_encoder_test_sigint_sync_wait_enabled(void)
{
    int wait_enabled;
    char const *sync_event;
    char const *sync_pid_text;
    char const *sync_fd_text;

    wait_enabled = 0;
    sync_event = NULL;
    sync_pid_text = NULL;
    sync_fd_text = NULL;

    sync_event = sixel_compat_getenv(SIXEL_ENCODER_SIGINT_SYNC_EVENT_ENVVAR);
    if (sync_event != NULL
            && sync_event[0] != '\0'
            && strcmp(sync_event,
                      SIXEL_ENCODER_SIGINT_SYNC_EVENT_DISPATCH_START)
                   != 0) {
        return wait_enabled;
    }

    sync_pid_text = sixel_compat_getenv(SIXEL_ENCODER_SIGINT_SYNC_PID_ENVVAR);
    sync_fd_text = sixel_compat_getenv(SIXEL_ENCODER_SIGINT_SYNC_FD_ENVVAR);
    if ((sync_pid_text != NULL && sync_pid_text[0] != '\0')
            || (sync_fd_text != NULL && sync_fd_text[0] != '\0')) {
        wait_enabled = 1;
    }

    return wait_enabled;
}

static void
sixel_encoder_handoff_trace_emit(
    sixel_encoder_frame_pipeline_t *pipeline,
    sixel_encoder_handoff_trace_event_t event,
    int frame_no,
    int loop_no,
    SIXELSTATUS status,
    sixel_encoder_handoff_trace_reason_t reason)
{
    char const *handoff_name;
    int minimal_enabled;

    handoff_name = "none";
    minimal_enabled = 0;
    if (pipeline != NULL) {
        handoff_name = sixel_encoder_handoff_mode_name(
            pipeline->handoff_mode);
    }
    minimal_enabled = sixel_encoder_handoff_trace_minimal_enabled();
    if (minimal_enabled != 0
            && event !=
                   SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_HANDOFF_DECIDE
            && event !=
                   SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_DISPATCH_START
            && event != SIXEL_ENCODER_HANDOFF_TRACE_EVENT_PIPELINE_STOP) {
        return;
    }
    sixel_trace_topic_message(
        SIXEL_TRACE_TOPIC_ENCODE_HANDOFF,
        "event=%s handoff=%s frame_no=%d loop_no=%d status=%d reason=%s",
        sixel_encoder_handoff_trace_event_name(event),
        handoff_name,
        frame_no,
        loop_no,
        status,
        sixel_encoder_handoff_trace_reason_name(reason));
}

#define SIXEL_ENCODER_FILTER_PLAN_MAX 16

typedef struct sixel_filter_plan_node {
    sixel_filter_t *filter;
    sixel_filter_kind_t kind;
} sixel_filter_plan_node_t;

typedef struct sixel_filter_plan {
    sixel_filter_plan_node_t nodes[SIXEL_ENCODER_FILTER_PLAN_MAX];
    int count;
} sixel_filter_plan_t;

static SIXELSTATUS sixel_encoder_palette_job_init(
    sixel_palette_async_job_t *job,
    sixel_allocator_t *allocator);
static SIXELSTATUS sixel_encoder_apply_palette_filter(
    sixel_encoder_t *encoder,
    sixel_frame_t **frame_slot,
    int allow_cache,
    sixel_dither_t **dither_out);
static int sixel_encoder_resolve_suboption_choice_value(
    sixel_suboption_assignment_t const *assignment,
    int *value_out);
static int sixel_encoder_parse_sample_target(char const *text,
                                             size_t *value_out);
static void sixel_encoder_palette_job_dispose(sixel_palette_async_job_t *job);
static SIXELSTATUS sixel_encoder_palette_job_launch(
    sixel_palette_async_job_t *job,
    sixel_frame_t *frame,
    int target_pixelformat,
    sixel_encoder_t *encoder);
static SIXELSTATUS sixel_encoder_palette_job_wait(
    sixel_palette_async_job_t *job,
    sixel_dither_t **dither_out);
static SIXELSTATUS sixel_encoder_frame_pipeline_init(
    sixel_encoder_frame_pipeline_t *pipeline,
    sixel_encoder_t *encoder,
    sixel_output_t *output);
static void sixel_encoder_frame_pipeline_dispose(
    sixel_encoder_frame_pipeline_t *pipeline);
static SIXELSTATUS sixel_encoder_frame_pipeline_enqueue(
    sixel_encoder_frame_pipeline_t *pipeline,
    sixel_frame_t *frame);
static SIXELSTATUS sixel_encoder_frame_pipeline_finish(
    sixel_encoder_frame_pipeline_t *pipeline);
static int sixel_encoder_frame_pipeline_worker(void *priv);
static char const *sixel_encoder_handoff_trace_reason_name(
    sixel_encoder_handoff_trace_reason_t reason);
static char const *sixel_encoder_handoff_trace_event_name(
    sixel_encoder_handoff_trace_event_t event);
static void sixel_encoder_handoff_trace_emit(
    sixel_encoder_frame_pipeline_t *pipeline,
    sixel_encoder_handoff_trace_event_t event,
    int frame_no,
    int loop_no,
    SIXELSTATUS status,
    sixel_encoder_handoff_trace_reason_t reason);
static void sixel_encoder_test_sigint_sync_wait_for_cancel(
    sixel_encoder_t *encoder);
static void sixel_encoder_frame_pipeline_request_stop_locked(
    sixel_encoder_frame_pipeline_t *pipeline,
    SIXELSTATUS status,
    sixel_encoder_handoff_trace_reason_t reason);
static void sixel_encoder_frame_pipeline_request_stop(
    sixel_encoder_frame_pipeline_t *pipeline,
    SIXELSTATUS status,
    sixel_encoder_handoff_trace_reason_t reason);
static SIXELSTATUS sixel_encoder_load_callback_resolve_handoff(
    sixel_encoder_frame_pipeline_t *pipeline,
    sixel_encoding_planner_t *planner,
    sixel_encoder_t *encoder,
    sixel_frame_t *frame);
static SIXELSTATUS sixel_encoder_load_callback_dispatch(
    sixel_encoder_load_context_t *context,
    sixel_frame_t *frame);
static int sixel_encoder_handoff_needs_preplan_clone(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame);
static void sixel_encoder_filter_plan_init(sixel_filter_plan_t *plan);
static void sixel_encoder_filter_plan_teardown(sixel_filter_plan_t *plan);
static SIXELSTATUS sixel_encoder_filter_plan_append(
    sixel_filter_plan_t *plan,
    sixel_filter_kind_t kind,
    const void *config,
    sixel_frame_t **slot,
    int input_pixelformat,
    int input_colorspace,
    int output_pixelformat,
    int output_colorspace,
    int total_units);
static SIXELSTATUS sixel_encoder_filter_plan_run(
    sixel_filter_plan_t *plan,
    sixel_allocator_t *allocator,
    sixel_timeline_logger_t *logger);
static void sixel_debug_print_palette(sixel_dither_t *dither);
static SIXELSTATUS sixel_encoder_output_without_macro(
    sixel_frame_t *frame,
    sixel_dither_t *dither,
    sixel_output_t *output,
    sixel_encoder_t *encoder,
    int frame_no,
    int loop_no,
    int delay);
static SIXELSTATUS sixel_encoder_output_with_macro(
    sixel_frame_t *frame,
    sixel_dither_t *dither,
    sixel_output_t *output,
    sixel_encoder_t *encoder,
    int frame_no,
    int loop_no,
    int delay);
static SIXELSTATUS sixel_encoder_compute_frame_buffer_bytes(
    int width,
    int height,
    int pixelformat,
    size_t *byte_count_out);

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
# if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#  include <io.h>
# endif
# if defined(_MSC_VER)
#   include <time.h>
# endif

#endif /* _WIN32 */

#define SIXEL_ENCODER_DELAY_POLL_SLICE_USEC 10000U

static unsigned int
sixel_encoder_compute_remaining_delay_usec(
    sixel_output_t *output,
    int delay_cs,
    int *elapsed_usec_out,
    unsigned int *target_usec_out)
{
    sixel_output_frame_delay_t delay;
    SIXELSTATUS status;

    if (elapsed_usec_out != NULL) {
        *elapsed_usec_out = 0;
    }
    if (target_usec_out != NULL) {
        *target_usec_out = 0U;
    }
    if (output == NULL || delay_cs <= 0) {
        return 0U;
    }

    status = sixel_output_compute_frame_delay(output, delay_cs, &delay);
    if (SIXEL_FAILED(status)) {
        return 0U;
    }

    if (target_usec_out != NULL) {
        *target_usec_out = delay.target_usec;
    }
    if (elapsed_usec_out != NULL) {
        *elapsed_usec_out = delay.elapsed_usec;
    }

    return delay.remaining_usec;
}

static SIXELSTATUS
sixel_encoder_set_encoder_core_format(sixel_output_t *output,
                                 int pixelformat,
                                 int source_colorspace,
                                 int colorspace)
{
    return sixel_output_set_encoder_format(output,
                                           pixelformat,
                                           source_colorspace,
                                           colorspace);
}

static int
sixel_encoder_is_cancel_requested(sixel_encoder_t const *encoder)
{
    int canceled;

    canceled = 0;
    if (encoder == NULL) {
        return 0;
    }
    if (encoder->cancel_function != NULL
            && encoder->cancel_function(encoder->cancel_context) != 0) {
        canceled = 1;
    }
    if (canceled == 0
            && encoder->cancel_flag != NULL
            && *encoder->cancel_flag != 0) {
        canceled = 1;
    }

    return canceled;
}

static void
sixel_encoder_test_sigint_sync_wait_for_cancel(sixel_encoder_t *encoder)
{
    int sync_wait_enabled;

    sync_wait_enabled = 0;
    if (encoder == NULL) {
        return;
    }

    sync_wait_enabled = sixel_encoder_test_sigint_sync_wait_enabled();
    if (sync_wait_enabled == 0) {
        return;
    }

    /*
     * Keep callback dispatch at a deterministic synchronization point while
     * running --sigint-run-until. The host sends SIGINT after observing the
     * dispatch marker; waiting here prevents tiny inputs from finishing before
     * cancel delivery can be observed by the loader callback.
     */
    while (sixel_encoder_is_cancel_requested(encoder) == 0) {
        sixel_sleep(1000u);
    }
}

static SIXELSTATUS
sixel_encoder_wait_delay_with_cancel(sixel_encoder_t *encoder,
                                     unsigned int delay_usec)
{
    unsigned int remaining_usec;
    unsigned int sleep_slice_usec;

    remaining_usec = 0U;
    sleep_slice_usec = 0U;

    if (encoder == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (delay_usec == 0U) {
        return SIXEL_OK;
    }

    /*
     * Process-level signals typically land on the main thread. Poll the
     * registered cancel source in short slices so worker-thread delay sleeps
     * can stop promptly after Ctrl-C.
     */
    remaining_usec = delay_usec;
    while (remaining_usec > 0U) {
        if (sixel_encoder_is_cancel_requested(encoder) != 0) {
            return SIXEL_INTERRUPTED;
        }
        sleep_slice_usec = remaining_usec;
        if (sleep_slice_usec > SIXEL_ENCODER_DELAY_POLL_SLICE_USEC) {
            sleep_slice_usec = SIXEL_ENCODER_DELAY_POLL_SLICE_USEC;
        }
        sixel_sleep(sleep_slice_usec);
        remaining_usec -= sleep_slice_usec;
    }

    if (sixel_encoder_is_cancel_requested(encoder) != 0) {
        return SIXEL_INTERRUPTED;
    }

    return SIXEL_OK;
}


static sixel_option_choice_t const g_option_choices_builtin_palette[] = {
    { "xterm16", SIXEL_BUILTIN_XTERM16 },
    { "xterm256", SIXEL_BUILTIN_XTERM256 },
    { "vt340mono", SIXEL_BUILTIN_VT340_MONO },
    { "vt340color", SIXEL_BUILTIN_VT340_COLOR },
    { "gray1", SIXEL_BUILTIN_G1 },
    { "gray2", SIXEL_BUILTIN_G2 },
    { "gray4", SIXEL_BUILTIN_G4 },
    { "gray8", SIXEL_BUILTIN_G8 }
};

static sixel_suboption_choice_t const g_option_choices_stbn_source[] = {
    { "hash", SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_HASH },
    { "mask", SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_MASK },
    { "pmj", SIXEL_INTERFRAME_STRATEGY_TOKEN_PMJ }
};

static sixel_suboption_choice_t const g_option_choices_toggle_01[] = {
    { "0", 0 },
    { "1", 1 }
};

static sixel_suboption_choice_t const g_option_choices_diffusion_scan[] = {
    { "auto", SIXEL_SCAN_AUTO },
    { "serpentine", SIXEL_SCAN_SERPENTINE },
    { "raster", SIXEL_SCAN_RASTER }
};

static sixel_suboption_choice_t const g_option_choices_bluenoise_channel[] = {
    { "mono", 0 },
    { "rgb", 1 }
};

static sixel_suboption_choice_t const g_option_choices_bluenoise_size[] = {
    { "64", 64 }
};

static sixel_suboption_choice_t const
g_option_choices_interframe_diffusion[] = {
    { "auto", SIXEL_DIFFUSE_FS },
    { "none", SIXEL_DIFFUSE_NONE },
    { "fs", SIXEL_DIFFUSE_FS },
    { "atkinson", SIXEL_DIFFUSE_ATKINSON },
    { "jajuni", SIXEL_DIFFUSE_JAJUNI },
    { "stucki", SIXEL_DIFFUSE_STUCKI },
    { "burkes", SIXEL_DIFFUSE_BURKES },
    { "sierra1", SIXEL_DIFFUSE_SIERRA1 },
    { "sierra2", SIXEL_DIFFUSE_SIERRA2 },
    { "sierra3", SIXEL_DIFFUSE_SIERRA3 }
};

static sixel_suboption_choice_t const g_option_choices_sierra_variant[] = {
    { "1", SIXEL_DIFFUSE_SIERRA1 },
    { "2", SIXEL_DIFFUSE_SIERRA2 },
    { "3", SIXEL_DIFFUSE_SIERRA3 }
};

static sixel_suboption_key_t const g_subkeys_diffusion_scan[] = {
    {
        "scan",
        NULL,
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_diffusion_scan,
        sizeof(g_option_choices_diffusion_scan)
        / sizeof(g_option_choices_diffusion_scan[0])
    }
};

static sixel_suboption_key_t const g_subkeys_diffusion_sierra[] = {
    {
        "variant",
        NULL,
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_sierra_variant,
        sizeof(g_option_choices_sierra_variant)
        / sizeof(g_option_choices_sierra_variant[0])
    },
    {
        "scan",
        NULL,
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_diffusion_scan,
        sizeof(g_option_choices_diffusion_scan)
        / sizeof(g_option_choices_diffusion_scan[0])
    }
};

static sixel_suboption_key_t const g_subkeys_diffusion_interframe[] = {
    {
        "diffusion",
        NULL,
        SIXEL_DITHER_INTERFRAME_DIFFUSION_ENVVAR,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_interframe_diffusion,
        sizeof(g_option_choices_interframe_diffusion)
        / sizeof(g_option_choices_interframe_diffusion[0])
    },
    {
        "scan",
        NULL,
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_diffusion_scan,
        sizeof(g_option_choices_diffusion_scan)
        / sizeof(g_option_choices_diffusion_scan[0])
    }
};

static sixel_suboption_key_t const g_subkeys_diffusion_stbn[] = {
    {
        "source",
        NULL,
        SIXEL_DITHER_STBN_SOURCE_ENVVAR,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_stbn_source,
        sizeof(g_option_choices_stbn_source)
        / sizeof(g_option_choices_stbn_source[0])
    },
    {
        "diffusion",
        NULL,
        SIXEL_DITHER_STBN_DIFFUSION_ENVVAR,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_interframe_diffusion,
        sizeof(g_option_choices_interframe_diffusion)
        / sizeof(g_option_choices_interframe_diffusion[0])
    },
    {
        "strength",
        NULL,
        SIXEL_DITHER_STBN_STRENGTH_ENVVAR,
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "scan",
        NULL,
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_diffusion_scan,
        sizeof(g_option_choices_diffusion_scan)
        / sizeof(g_option_choices_diffusion_scan[0])
    },
    {
        "motion_adapt",
        NULL,
        SIXEL_DITHER_STBN_MOTION_ADAPT_ENVVAR,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_toggle_01,
        sizeof(g_option_choices_toggle_01)
        / sizeof(g_option_choices_toggle_01[0])
    },
    {
        "scene_cut_reset",
        NULL,
        SIXEL_DITHER_STBN_SCENE_CUT_RESET_ENVVAR,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_toggle_01,
        sizeof(g_option_choices_toggle_01)
        / sizeof(g_option_choices_toggle_01[0])
    },
    {
        "scene_detect",
        NULL,
        SIXEL_DITHER_STBN_SCENE_DETECT_ENVVAR,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_toggle_01,
        sizeof(g_option_choices_toggle_01)
        / sizeof(g_option_choices_toggle_01[0])
    },
    {
        "alpha_guard",
        NULL,
        SIXEL_DITHER_STBN_ALPHA_GUARD_ENVVAR,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_toggle_01,
        sizeof(g_option_choices_toggle_01)
        / sizeof(g_option_choices_toggle_01[0])
    },
    {
        "perceptual_weight",
        NULL,
        SIXEL_DITHER_STBN_PERCEPTUAL_WEIGHT_ENVVAR,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_toggle_01,
        sizeof(g_option_choices_toggle_01)
        / sizeof(g_option_choices_toggle_01[0])
    },
    {
        "fastpath",
        NULL,
        SIXEL_DITHER_STBN_FASTPATH_ENVVAR,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_toggle_01,
        sizeof(g_option_choices_toggle_01)
        / sizeof(g_option_choices_toggle_01[0])
    }
};

static sixel_suboption_key_t const g_subkeys_diffusion_bluenoise[] = {
    {
        "strength",
        NULL,
        "SIXEL_DITHER_BLUENOISE_STRENGTH",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "gradient_factor",
        "G",
        "SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "phase",
        NULL,
        "SIXEL_DITHER_BLUENOISE_PHASE",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "seed",
        NULL,
        "SIXEL_DITHER_BLUENOISE_SEED",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "channel",
        NULL,
        "SIXEL_DITHER_BLUENOISE_CHANNEL",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_bluenoise_channel,
        sizeof(g_option_choices_bluenoise_channel)
        / sizeof(g_option_choices_bluenoise_channel[0])
    },
    {
        "size",
        NULL,
        "SIXEL_DITHER_BLUENOISE_SIZE",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_bluenoise_size,
        sizeof(g_option_choices_bluenoise_size)
        / sizeof(g_option_choices_bluenoise_size[0])
    },
    {
        "scan",
        NULL,
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_diffusion_scan,
        sizeof(g_option_choices_diffusion_scan)
        / sizeof(g_option_choices_diffusion_scan[0])
    }
};

enum {
    /*
     * Keep this as an enum constant so static initializers stay valid on
     * strict compilers (MSVC/pcc reject file-scope const objects here).
     */
    G_SUBKEYS_DIFFUSION_INTERFRAME_COUNT =
        (int)(sizeof(g_subkeys_diffusion_interframe)
              / sizeof(g_subkeys_diffusion_interframe[0])),
    G_SUBKEYS_DIFFUSION_SCAN_COUNT =
        (int)(sizeof(g_subkeys_diffusion_scan)
              / sizeof(g_subkeys_diffusion_scan[0])),
    G_SUBKEYS_DIFFUSION_SIERRA_COUNT =
        (int)(sizeof(g_subkeys_diffusion_sierra)
              / sizeof(g_subkeys_diffusion_sierra[0])),
    G_SUBKEYS_DIFFUSION_STBN_COUNT =
        (int)(sizeof(g_subkeys_diffusion_stbn)
              / sizeof(g_subkeys_diffusion_stbn[0])),
    G_SUBKEYS_DIFFUSION_BLUENOISE_COUNT =
        (int)(sizeof(g_subkeys_diffusion_bluenoise)
              / sizeof(g_subkeys_diffusion_bluenoise[0]))
};

static sixel_option_value_schema_t const g_schema_diffusion_values[] = {
    {
        "auto",
        SIXEL_DIFFUSE_AUTO,
        g_subkeys_diffusion_scan,
        G_SUBKEYS_DIFFUSION_SCAN_COUNT
    },
    {
        "none",
        SIXEL_DIFFUSE_NONE,
        g_subkeys_diffusion_scan,
        G_SUBKEYS_DIFFUSION_SCAN_COUNT
    },
    {
        "fs",
        SIXEL_DIFFUSE_FS,
        g_subkeys_diffusion_scan,
        G_SUBKEYS_DIFFUSION_SCAN_COUNT
    },
    {
        "atkinson",
        SIXEL_DIFFUSE_ATKINSON,
        g_subkeys_diffusion_scan,
        G_SUBKEYS_DIFFUSION_SCAN_COUNT
    },
    {
        "jajuni",
        SIXEL_DIFFUSE_JAJUNI,
        g_subkeys_diffusion_scan,
        G_SUBKEYS_DIFFUSION_SCAN_COUNT
    },
    {
        "stucki",
        SIXEL_DIFFUSE_STUCKI,
        g_subkeys_diffusion_scan,
        G_SUBKEYS_DIFFUSION_SCAN_COUNT
    },
    {
        "burkes",
        SIXEL_DIFFUSE_BURKES,
        g_subkeys_diffusion_scan,
        G_SUBKEYS_DIFFUSION_SCAN_COUNT
    },
    {
        "sierra",
        SIXEL_DIFFUSE_SIERRA1,
        g_subkeys_diffusion_sierra,
        G_SUBKEYS_DIFFUSION_SIERRA_COUNT
    },
    {
        "a_dither",
        SIXEL_DIFFUSE_A_DITHER,
        g_subkeys_diffusion_scan,
        G_SUBKEYS_DIFFUSION_SCAN_COUNT
    },
    {
        "x_dither",
        SIXEL_DIFFUSE_X_DITHER,
        g_subkeys_diffusion_scan,
        G_SUBKEYS_DIFFUSION_SCAN_COUNT
    },
    {
        "bluenoise",
        SIXEL_DIFFUSE_BLUENOISE_DITHER,
        g_subkeys_diffusion_bluenoise,
        G_SUBKEYS_DIFFUSION_BLUENOISE_COUNT
    },
    {
        "lso2",
        SIXEL_DIFFUSE_LSO2,
        g_subkeys_diffusion_scan,
        G_SUBKEYS_DIFFUSION_SCAN_COUNT
    },
    {
        "interframe",
        SIXEL_DIFFUSE_INTERFRAME,
        g_subkeys_diffusion_interframe,
        G_SUBKEYS_DIFFUSION_INTERFRAME_COUNT
    },
    {
        "stbn",
        SIXEL_DIFFUSE_INTERFRAME,
        g_subkeys_diffusion_stbn,
        G_SUBKEYS_DIFFUSION_STBN_COUNT
    }
};

static sixel_option_argument_schema_t const g_schema_diffusion = {
    SIXEL_OPTFLAG_DIFFUSION,
    "--diffusion",
    g_schema_diffusion_values,
    sizeof(g_schema_diffusion_values) / sizeof(g_schema_diffusion_values[0])
};

static sixel_option_choice_t const g_option_choices_find_largest[] = {
    { "auto", SIXEL_LARGE_AUTO },
    { "norm", SIXEL_LARGE_NORM },
    { "lum", SIXEL_LARGE_LUM },
    { "pca", SIXEL_LARGE_PCA }
};

static sixel_option_choice_t const g_option_choices_select_color[] = {
    { "auto", SIXEL_REP_AUTO },
    { "center", SIXEL_REP_CENTER_BOX },
    { "average", SIXEL_REP_AVERAGE_COLORS },
    { "histogram", SIXEL_REP_AVERAGE_PIXELS },
    { "histogram", SIXEL_REP_AVERAGE_PIXELS }
};

static sixel_suboption_choice_t const g_option_choices_kmeans_init_type[] = {
    { "auto", SIXEL_PALETTE_KMEANS_INIT_AUTO },
    { "none", SIXEL_PALETTE_KMEANS_INIT_NONE },
    { "pca", SIXEL_PALETTE_KMEANS_INIT_PCA }
};

static sixel_suboption_choice_t const g_option_choices_kmeans_binning[] = {
    { "auto", SIXEL_PALETTE_KMEANS_BINNING_AUTO },
    { "none", SIXEL_PALETTE_KMEANS_BINNING_NONE },
    { "hard", SIXEL_PALETTE_KMEANS_BINNING_HARD },
    { "soft", SIXEL_PALETTE_KMEANS_BINNING_SOFT }
};

static sixel_suboption_choice_t const g_option_choices_kmeans_mapping[] = {
    { "uniform", SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM },
    { "srgb", SIXEL_PALETTE_KMEANS_MAPPING_SRGB }
};

static sixel_suboption_choice_t const g_option_choices_kmeans_softdist[] = {
    { "trilinear", SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR }
};

static sixel_suboption_choice_t const g_option_choices_kmeans_feedback[] = {
    { "off", SIXEL_PALETTE_KMEANS_FEEDBACK_OFF },
    { "on", SIXEL_PALETTE_KMEANS_FEEDBACK_ON }
};

static sixel_suboption_choice_t const g_option_choices_kmeans_prune[] = {
    { "auto", SIXEL_PALETTE_KMEANS_PRUNE_AUTO },
    { "none", SIXEL_PALETTE_KMEANS_PRUNE_NONE },
    { "hamerly", SIXEL_PALETTE_KMEANS_PRUNE_HAMERLY },
    { "elkan", SIXEL_PALETTE_KMEANS_PRUNE_ELKAN },
    { "yinyang", SIXEL_PALETTE_KMEANS_PRUNE_YINYANG }
};

static sixel_suboption_choice_t const g_option_choices_kmedoids_algo[] = {
    { "auto", SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO },
    { "pam", SIXEL_PALETTE_KMEDOIDS_ALGO_PAM },
    { "sample", SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA },
    { "random", SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS },
    { "bandit", SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM }
};

static sixel_suboption_choice_t const g_option_choices_kcenter_algo[] = {
    { "auto", SIXEL_PALETTE_KCENTER_ALGO_AUTO },
    { "fft", SIXEL_PALETTE_KCENTER_ALGO_FFT },
    { "swap", SIXEL_PALETTE_KCENTER_ALGO_SWAP },
    { "hybrid", SIXEL_PALETTE_KCENTER_ALGO_HYBRID }
};

static sixel_suboption_choice_t const g_option_choices_kcenter_profile[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_PROFILE_LEGACY },
    { "speed", SIXEL_PALETTE_KCENTER_PROFILE_SPEED },
    { "balance", SIXEL_PALETTE_KCENTER_PROFILE_BALANCE },
    { "quality", SIXEL_PALETTE_KCENTER_PROFILE_QUALITY }
};

static sixel_suboption_choice_t const g_option_choices_kcenter_auto_policy[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY },
    { "adaptive", SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE }
};

static sixel_suboption_choice_t const g_option_choices_kcenter_space_policy[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY },
    { "perceptual", SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL }
};

static sixel_suboption_choice_t const
g_option_choices_kcenter_candidate_policy[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY },
    { "hybrid", SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID }
};

static sixel_suboption_choice_t const g_option_choices_kcenter_budget_policy[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY },
    { "adaptive", SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE }
};

static sixel_suboption_choice_t const g_option_choices_kcenter_swap_update[] = {
    { "full", SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL },
    { "incremental", SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL }
};

static sixel_suboption_choice_t const g_option_choices_quantize_merge[] = {
    { "auto", SIXEL_FINAL_MERGE_AUTO },
    { "none", SIXEL_FINAL_MERGE_NONE },
    { "ward", SIXEL_FINAL_MERGE_WARD }
};

static sixel_suboption_choice_t const
g_option_choices_heckbert_profile[] = {
    { "compat", SIXEL_HECKBERT_PROFILE_COMPAT },
    { "speed", SIXEL_HECKBERT_PROFILE_SPEED },
    { "quality", SIXEL_HECKBERT_PROFILE_QUALITY }
};

static sixel_suboption_key_t const g_subkeys_quantize_model_merge_only[] = {
    {
        "animation_mode",
        NULL,
        SIXEL_PALETTE_ANIMATION_MODE_ENVVAR,
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "scene_cut_threshold",
        NULL,
        SIXEL_PALETTE_SCENE_CUT_THRESHOLD_ENVVAR,
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "merge",
        "G",
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_quantize_merge,
        sizeof(g_option_choices_quantize_merge)
        / sizeof(g_option_choices_quantize_merge[0])
    },
    {
        "merge_oversplit",
        "O",
        "SIXEL_PALETTE_OVERSPLIT_FACTOR",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "merge_lloyd",
        "L",
        "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    }
};

static sixel_suboption_key_t const g_subkeys_quantize_model_heckbert[] = {
    {
        "animation_mode",
        NULL,
        SIXEL_PALETTE_ANIMATION_MODE_ENVVAR,
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "scene_cut_threshold",
        NULL,
        SIXEL_PALETTE_SCENE_CUT_THRESHOLD_ENVVAR,
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "profile",
        NULL,
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_heckbert_profile,
        sizeof(g_option_choices_heckbert_profile)
        / sizeof(g_option_choices_heckbert_profile[0])
    },
    {
        "merge",
        "G",
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_quantize_merge,
        sizeof(g_option_choices_quantize_merge)
        / sizeof(g_option_choices_quantize_merge[0])
    },
    {
        "merge_oversplit",
        "O",
        "SIXEL_PALETTE_OVERSPLIT_FACTOR",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "merge_lloyd",
        "L",
        "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    }
};

static sixel_suboption_key_t const g_subkeys_quantize_model_kmeans[] = {
    {
        "inittype",
        "I",
        "SIXEL_PALETTE_KMEANS_INITTYPE",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kmeans_init_type,
        sizeof(g_option_choices_kmeans_init_type)
        / sizeof(g_option_choices_kmeans_init_type[0])
    },
    {
        "threshold",
        "T",
        "SIXEL_PALETTE_KMEANS_THRESHOLD",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "binning",
        "B",
        "SIXEL_PALETTE_KMEANS_BINNING",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kmeans_binning,
        sizeof(g_option_choices_kmeans_binning)
        / sizeof(g_option_choices_kmeans_binning[0])
    },
    {
        "binbits",
        "N",
        "SIXEL_PALETTE_KMEANS_BINBITS",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "mapping",
        "M",
        "SIXEL_PALETTE_KMEANS_MAPPING",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kmeans_mapping,
        sizeof(g_option_choices_kmeans_mapping)
        / sizeof(g_option_choices_kmeans_mapping[0])
    },
    {
        "softdist",
        "D",
        "SIXEL_PALETTE_KMEANS_SOFTDIST",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kmeans_softdist,
        sizeof(g_option_choices_kmeans_softdist)
        / sizeof(g_option_choices_kmeans_softdist[0])
    },
    {
        "autoratio",
        "R",
        "SIXEL_PALETTE_KMEANS_AUTORATIO",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "feedback",
        "F",
        "SIXEL_PALETTE_KMEANS_FEEDBACK",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kmeans_feedback,
        sizeof(g_option_choices_kmeans_feedback)
        / sizeof(g_option_choices_kmeans_feedback[0])
    },
    {
        "prune",
        NULL,
        "SIXEL_PALETTE_KMEANS_PRUNE",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kmeans_prune,
        sizeof(g_option_choices_kmeans_prune)
        / sizeof(g_option_choices_kmeans_prune[0])
    },
    {
        "seed",
        "S",
        "SIXEL_PALETTE_KMEANS_SEED",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "restarts",
        NULL,
        "SIXEL_PALETTE_KMEANS_RESTARTS",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "iter",
        NULL,
        "SIXEL_PALETTE_KMEANS_ITER",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "iter_max",
        NULL,
        "SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "miniter",
        NULL,
        "SIXEL_PALETTE_KMEANS_MINITER",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "polish_iter",
        NULL,
        "SIXEL_PALETTE_KMEANS_POLISH_ITER",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "feedback_slots",
        NULL,
        "SIXEL_PALETTE_KMEANS_FEEDBACK_SLOTS",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "feedback_interval",
        NULL,
        "SIXEL_PALETTE_KMEANS_FEEDBACK_INTERVAL",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "animation_mode",
        NULL,
        SIXEL_PALETTE_ANIMATION_MODE_ENVVAR,
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "scene_cut_threshold",
        NULL,
        SIXEL_PALETTE_SCENE_CUT_THRESHOLD_ENVVAR,
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "merge",
        "G",
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_quantize_merge,
        sizeof(g_option_choices_quantize_merge)
        / sizeof(g_option_choices_quantize_merge[0])
    },
    {
        "merge_oversplit",
        "O",
        "SIXEL_PALETTE_OVERSPLIT_FACTOR",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "merge_lloyd",
        "L",
        "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    }
};

static sixel_suboption_key_t const g_subkeys_quantize_model_kmedoids[] = {
    {
        "algo",
        "A",
        "SIXEL_PALETTE_KMEDOIDS_ALGO",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kmedoids_algo,
        sizeof(g_option_choices_kmedoids_algo)
        / sizeof(g_option_choices_kmedoids_algo[0])
    },
    {
        "seed",
        "S",
        "SIXEL_PALETTE_KMEDOIDS_SEED",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "iter",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_ITER",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "sample",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_SAMPLE",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "clara_trials",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_CLARA_TRIALS",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "clara_sample",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_CLARA_SAMPLE",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "clarans_local",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_CLARANS_LOCAL",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "clarans_neighbors",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_CLARANS_NEIGHBORS",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "bandit_iter",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_ITER",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "bandit_candidates",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_CANDIDATES",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "bandit_batch",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_BATCH",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "histbits",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_HISTBITS",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "point_budget",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_POINT_BUDGET",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "rare_keep",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_RARE_KEEP",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "prune_mass",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_PRUNE_MASS",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "auction",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_AUCTION",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_toggle_01,
        sizeof(g_option_choices_toggle_01)
        / sizeof(g_option_choices_toggle_01[0])
    },
    {
        "auction_shortlist",
        NULL,
        "SIXEL_PALETTE_KMEDOIDS_AUCTION_SHORTLIST",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "animation_mode",
        NULL,
        SIXEL_PALETTE_ANIMATION_MODE_ENVVAR,
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "scene_cut_threshold",
        NULL,
        SIXEL_PALETTE_SCENE_CUT_THRESHOLD_ENVVAR,
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "merge",
        "G",
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_quantize_merge,
        sizeof(g_option_choices_quantize_merge)
        / sizeof(g_option_choices_quantize_merge[0])
    },
    {
        "merge_oversplit",
        "O",
        "SIXEL_PALETTE_OVERSPLIT_FACTOR",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "merge_lloyd",
        "L",
        "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    }
};

static sixel_suboption_key_t const g_subkeys_quantize_model_center[] = {
    {
        "algo",
        "A",
        "SIXEL_PALETTE_KCENTER_ALGO",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kcenter_algo,
        sizeof(g_option_choices_kcenter_algo)
        / sizeof(g_option_choices_kcenter_algo[0])
    },
    {
        "profile",
        NULL,
        "SIXEL_PALETTE_KCENTER_PROFILE",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kcenter_profile,
        sizeof(g_option_choices_kcenter_profile)
        / sizeof(g_option_choices_kcenter_profile[0])
    },
    {
        "seed",
        "S",
        "SIXEL_PALETTE_KCENTER_SEED",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "auto_policy",
        NULL,
        "SIXEL_PALETTE_KCENTER_AUTO_POLICY",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kcenter_auto_policy,
        sizeof(g_option_choices_kcenter_auto_policy)
        / sizeof(g_option_choices_kcenter_auto_policy[0])
    },
    {
        "auto_fft_threshold",
        NULL,
        "SIXEL_PALETTE_KCENTER_AUTO_FFT_THRESHOLD",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "space_policy",
        NULL,
        "SIXEL_PALETTE_KCENTER_SPACE_POLICY",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kcenter_space_policy,
        sizeof(g_option_choices_kcenter_space_policy)
        / sizeof(g_option_choices_kcenter_space_policy[0])
    },
    {
        "candidate_policy",
        NULL,
        "SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kcenter_candidate_policy,
        sizeof(g_option_choices_kcenter_candidate_policy)
        / sizeof(g_option_choices_kcenter_candidate_policy[0])
    },
    {
        "restarts",
        NULL,
        "SIXEL_PALETTE_KCENTER_RESTARTS",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "init_seeds",
        NULL,
        "SIXEL_PALETTE_KCENTER_INIT_SEEDS",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "iter",
        NULL,
        "SIXEL_PALETTE_KCENTER_ITER",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "histbits",
        NULL,
        "SIXEL_PALETTE_KCENTER_HISTBITS",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "point_budget",
        NULL,
        "SIXEL_PALETTE_KCENTER_POINT_BUDGET",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "rare_keep",
        NULL,
        "SIXEL_PALETTE_KCENTER_RARE_KEEP",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "prune_mass",
        NULL,
        "SIXEL_PALETTE_KCENTER_PRUNE_MASS",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "budget_policy",
        NULL,
        "SIXEL_PALETTE_KCENTER_BUDGET_POLICY",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kcenter_budget_policy,
        sizeof(g_option_choices_kcenter_budget_policy)
        / sizeof(g_option_choices_kcenter_budget_policy[0])
    },
    {
        "budget_scale",
        NULL,
        "SIXEL_PALETTE_KCENTER_BUDGET_SCALE",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "swap_topk",
        NULL,
        "SIXEL_PALETTE_KCENTER_SWAP_TOPK",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "swap_update",
        NULL,
        "SIXEL_PALETTE_KCENTER_SWAP_UPDATE",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kcenter_swap_update,
        sizeof(g_option_choices_kcenter_swap_update)
        / sizeof(g_option_choices_kcenter_swap_update[0])
    },
    {
        "swap_patience",
        NULL,
        "SIXEL_PALETTE_KCENTER_SWAP_PATIENCE",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "swap_min_gain",
        NULL,
        "SIXEL_PALETTE_KCENTER_SWAP_MIN_GAIN",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "animation_mode",
        NULL,
        SIXEL_PALETTE_ANIMATION_MODE_ENVVAR,
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "scene_cut_threshold",
        NULL,
        SIXEL_PALETTE_SCENE_CUT_THRESHOLD_ENVVAR,
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "merge",
        "G",
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_quantize_merge,
        sizeof(g_option_choices_quantize_merge)
        / sizeof(g_option_choices_quantize_merge[0])
    },
    {
        "merge_oversplit",
        "O",
        "SIXEL_PALETTE_OVERSPLIT_FACTOR",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "merge_lloyd",
        "L",
        "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    }
};

static sixel_option_value_schema_t const g_schema_quantize_model_values[] = {
    {
        "auto",
        SIXEL_QUANTIZE_MODEL_AUTO,
        g_subkeys_quantize_model_merge_only,
        sizeof(g_subkeys_quantize_model_merge_only)
        / sizeof(g_subkeys_quantize_model_merge_only[0])
    },
    {
        "heckbert",
        SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        g_subkeys_quantize_model_heckbert,
        sizeof(g_subkeys_quantize_model_heckbert)
        / sizeof(g_subkeys_quantize_model_heckbert[0])
    },
    {
        "kmeans",
        SIXEL_QUANTIZE_MODEL_KMEANS,
        g_subkeys_quantize_model_kmeans,
        sizeof(g_subkeys_quantize_model_kmeans)
        / sizeof(g_subkeys_quantize_model_kmeans[0])
    },
    {
        "medoids",
        SIXEL_QUANTIZE_MODEL_KMEDOIDS,
        g_subkeys_quantize_model_kmedoids,
        sizeof(g_subkeys_quantize_model_kmedoids)
        / sizeof(g_subkeys_quantize_model_kmedoids[0])
    },
    {
        "center",
        SIXEL_QUANTIZE_MODEL_KCENTER,
        g_subkeys_quantize_model_center,
        sizeof(g_subkeys_quantize_model_center)
        / sizeof(g_subkeys_quantize_model_center[0])
    }
};

static sixel_option_argument_schema_t const g_schema_quantize_model = {
    SIXEL_OPTFLAG_QUANTIZE_MODEL,
    "--quantize-model",
    g_schema_quantize_model_values,
    sizeof(g_schema_quantize_model_values)
    / sizeof(g_schema_quantize_model_values[0])
};

static sixel_option_choice_t const g_option_choices_resampling[] = {
    { "nearest", SIXEL_RES_NEAREST },
    { "gaussian", SIXEL_RES_GAUSSIAN },
    { "hanning", SIXEL_RES_HANNING },
    { "hamming", SIXEL_RES_HAMMING },
    { "bilinear", SIXEL_RES_BILINEAR },
    { "welsh", SIXEL_RES_WELSH },
    { "bicubic", SIXEL_RES_BICUBIC },
    { "lanczos2", SIXEL_RES_LANCZOS2 },
    { "lanczos3", SIXEL_RES_LANCZOS3 },
    { "lanczos4", SIXEL_RES_LANCZOS4 }
};

static sixel_option_choice_t const g_option_choices_quality[] = {
    { "auto", SIXEL_QUALITY_AUTO },
    { "high", SIXEL_QUALITY_HIGH },
    { "low", SIXEL_QUALITY_LOW },
    { "full", SIXEL_QUALITY_FULL }
};

static sixel_option_choice_t const g_option_choices_loopmode[] = {
    { "auto", SIXEL_LOOP_AUTO },
    { "force", SIXEL_LOOP_FORCE },
    { "disable", SIXEL_LOOP_DISABLE }
};

static sixel_option_choice_t const g_option_choices_palette_type[] = {
    { "auto", SIXEL_PALETTETYPE_AUTO },
    { "hls", SIXEL_PALETTETYPE_HLS },
    { "rgb", SIXEL_PALETTETYPE_RGB }
};

static sixel_option_choice_t const g_option_choices_encode_policy[] = {
    { "auto", SIXEL_ENCODEPOLICY_AUTO },
    { "fast", SIXEL_ENCODEPOLICY_FAST },
    { "size", SIXEL_ENCODEPOLICY_SIZE }
};

static sixel_option_choice_t const g_option_choices_lut_policy[] = {
    { "auto", SIXEL_LUT_POLICY_AUTO },
    { "5bit", SIXEL_LUT_POLICY_5BIT },
    { "6bit", SIXEL_LUT_POLICY_6BIT },
    { "none", SIXEL_LUT_POLICY_NONE },
    { "certlut", SIXEL_LUT_POLICY_CERTLUT },
    { "eytzinger", SIXEL_LUT_POLICY_EYTZINGER },
    { "fhedt", SIXEL_LUT_POLICY_FHEDT },
    { "vptree", SIXEL_LUT_POLICY_VPTREE },
    { "rbc", SIXEL_LUT_POLICY_RBC },
    { "mahalanobis", SIXEL_LUT_POLICY_MAHALANOBIS }
};

static sixel_suboption_choice_t const
g_option_choices_lut_shared_instance[] = {
    { "0", 0 },
    { "1", 1 }
};

static sixel_suboption_key_t const g_subkeys_lookup_policy_shared[] = {
    {
        "shared_instance",
        NULL,
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_lut_shared_instance,
        sizeof(g_option_choices_lut_shared_instance)
        / sizeof(g_option_choices_lut_shared_instance[0])
    }
};

enum {
    G_SUBKEYS_LOOKUP_POLICY_SHARED_COUNT =
        (int)(sizeof(g_subkeys_lookup_policy_shared)
              / sizeof(g_subkeys_lookup_policy_shared[0]))
};

static sixel_option_value_schema_t const g_schema_lookup_policy_values[] = {
    {
        "auto",
        SIXEL_LUT_POLICY_AUTO,
        NULL,
        0u
    },
    {
        "5bit",
        SIXEL_LUT_POLICY_5BIT,
        g_subkeys_lookup_policy_shared,
        G_SUBKEYS_LOOKUP_POLICY_SHARED_COUNT
    },
    {
        "6bit",
        SIXEL_LUT_POLICY_6BIT,
        g_subkeys_lookup_policy_shared,
        G_SUBKEYS_LOOKUP_POLICY_SHARED_COUNT
    },
    {
        "none",
        SIXEL_LUT_POLICY_NONE,
        NULL,
        0u
    },
    {
        "certlut",
        SIXEL_LUT_POLICY_CERTLUT,
        g_subkeys_lookup_policy_shared,
        G_SUBKEYS_LOOKUP_POLICY_SHARED_COUNT
    },
    {
        "eytzinger",
        SIXEL_LUT_POLICY_EYTZINGER,
        NULL,
        0u
    },
    {
        "fhedt",
        SIXEL_LUT_POLICY_FHEDT,
        NULL,
        0u
    },
    {
        "vptree",
        SIXEL_LUT_POLICY_VPTREE,
        NULL,
        0u
    },
    {
        "rbc",
        SIXEL_LUT_POLICY_RBC,
        NULL,
        0u
    },
    {
        "mahalanobis",
        SIXEL_LUT_POLICY_MAHALANOBIS,
        NULL,
        0u
    }
};

static sixel_option_argument_schema_t const g_schema_lookup_policy = {
    SIXEL_OPTFLAG_LUT_POLICY,
    "--lookup-policy",
    g_schema_lookup_policy_values,
    sizeof(g_schema_lookup_policy_values)
    / sizeof(g_schema_lookup_policy_values[0])
};

static sixel_option_choice_t const g_option_choices_working_colorspace[] = {
    { "gamma", SIXEL_COLORSPACE_GAMMA },
    { "linear", SIXEL_COLORSPACE_LINEAR },
    { "oklab", SIXEL_COLORSPACE_OKLAB },
    { "cielab", SIXEL_COLORSPACE_CIELAB },
    { "din99d", SIXEL_COLORSPACE_DIN99D }
};

static sixel_option_choice_t const g_option_choices_output_colorspace[] = {
    { "gamma", SIXEL_COLORSPACE_GAMMA },
    { "linear", SIXEL_COLORSPACE_LINEAR },
    { "smpte-c", SIXEL_COLORSPACE_SMPTEC },
    { "smptec", SIXEL_COLORSPACE_SMPTEC }
};

static int
sixel_encoder_pixelformat_for_colorspace(int colorspace,
                                         int prefer_float32)
{
    switch (colorspace) {
    case SIXEL_COLORSPACE_LINEAR:
        return SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    case SIXEL_COLORSPACE_OKLAB:
        return SIXEL_PIXELFORMAT_OKLABFLOAT32;
    case SIXEL_COLORSPACE_CIELAB:
        return SIXEL_PIXELFORMAT_CIELABFLOAT32;
    case SIXEL_COLORSPACE_DIN99D:
        return SIXEL_PIXELFORMAT_DIN99DFLOAT32;
    default:
        if (prefer_float32) {
            return SIXEL_PIXELFORMAT_RGBFLOAT32;
        }
        return SIXEL_PIXELFORMAT_RGB888;
    }
}

static sixel_option_choice_t const g_option_choices_precision[] = {
    { "auto", SIXEL_ENCODER_PRECISION_MODE_AUTO },
    { "8bit", SIXEL_ENCODER_PRECISION_MODE_8BIT },
    { "float32", SIXEL_ENCODER_PRECISION_MODE_FLOAT32 }
};

static char *
arg_strdup(
    char const          /* in */ *s,          /* source buffer */
    sixel_allocator_t   /* in */ *allocator)  /* allocator object for
                                                 destination buffer */
{
    char *p;
    size_t len;

    len = strlen(s);

    p = (char *)sixel_allocator_malloc(allocator, len + 1);
    if (p) {
        (void)sixel_compat_strcpy(p, len + 1, s);
    }
    return p;
}

static SIXELSTATUS
sixel_encoder_parse_loader_order(
    sixel_allocator_t /* in */ *allocator,
    char const        /* in */ *value,
    char              /* out */ **order_out)
{
    SIXELSTATUS status;
    char match_detail[128];
    char *output;
    sixel_option_argument_list_resolution_t parsed;

    status = SIXEL_OK;
    match_detail[0] = '\0';
    output = NULL;
    sixel_option_init_argument_list_resolution(&parsed);

    if (order_out != NULL) {
        *order_out = NULL;
    }

    if (value == NULL || value[0] == '\0') {
        return SIXEL_OK;
    }

    status = sixel_loader_order_parse_and_validate(
        value,
        &parsed,
        match_detail,
        sizeof(match_detail));
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    output = arg_strdup(parsed.canonical_argument, allocator);
    if (output == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_parse_loader_order: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    if (order_out != NULL) {
        *order_out = output;
        output = NULL;
    }

cleanup:
    sixel_option_free_argument_list_resolution(&parsed);
    if (output != NULL) {
        sixel_allocator_free(allocator, output);
    }

    return status;
}

/*
 * Shared handoff frames can be touched by the loader while worker threads run.
 * Build a per-call planner snapshot so the decision stays thread-safe without
 * reading/writing encoder->planner from multiple threads.
 */
static int
sixel_encoder_handoff_needs_preplan_clone(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame)
{
    sixel_encoding_planner_t planner_snapshot;

    if (encoder == NULL || frame == NULL) {
        return 1;
    }

    sixel_encoding_planner_init(&planner_snapshot);
    sixel_encoding_planner_reset_for_frame(&planner_snapshot);
    sixel_encoding_planner_plan(&planner_snapshot, encoder, frame);

    if (planner_snapshot.clip_active != 0
        || planner_snapshot.scale_active != 0
        || planner_snapshot.colorspace_active != 0) {
        return 1;
    }

    return 0;
}

/*
 * Keep encoder-side frame access on IFrame.  The legacy public frame APIs
 * still exist for source compatibility, but storage-only helpers must stay
 * private to frame.c.
 */
static SIXELSTATUS
sixel_encoder_frame_get_pixels_view(sixel_frame_t const *frame,
                                    sixel_frame_pixels_view_t *view)
{
    sixel_frame_interface_t *frame_if;

    frame_if = NULL;
    if (frame == NULL || view == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame_if = sixel_frame_as_interface(frame);
    if (frame_if->vtbl == NULL || frame_if->vtbl->get_pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return frame_if->vtbl->get_pixels(frame_if, view);
}

static SIXELSTATUS
sixel_encoder_frame_get_transparency(
    sixel_frame_t const *frame,
    sixel_frame_transparency_t *transparency)
{
    sixel_frame_interface_t *frame_if;

    frame_if = NULL;
    if (frame == NULL || transparency == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame_if = sixel_frame_as_interface(frame);
    if (frame_if->vtbl == NULL ||
        frame_if->vtbl->get_transparency == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return frame_if->vtbl->get_transparency(frame_if, transparency);
}

static SIXELSTATUS
sixel_encoder_frame_set_transparency(
    sixel_frame_t *frame,
    sixel_frame_transparency_t const *transparency)
{
    sixel_frame_interface_t *frame_if;

    frame_if = NULL;
    if (frame == NULL || transparency == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame_if = sixel_frame_as_interface(frame);
    if (frame_if->vtbl == NULL ||
        frame_if->vtbl->set_transparency == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return frame_if->vtbl->set_transparency(frame_if, transparency);
}

static SIXELSTATUS
sixel_encoder_frame_clone(sixel_frame_t const *frame,
                          sixel_allocator_t *allocator,
                          sixel_frame_t **frame_out)
{
    SIXELSTATUS status;
    sixel_frame_interface_t *frame_if;
    sixel_frame_interface_t *clone_if;

    status = SIXEL_BAD_ARGUMENT;
    frame_if = NULL;
    clone_if = NULL;
    if (frame == NULL || frame_out == NULL) {
        return status;
    }
    *frame_out = NULL;

    frame_if = sixel_frame_as_interface(frame);
    if (frame_if->vtbl == NULL || frame_if->vtbl->clone == NULL) {
        return status;
    }

    status = frame_if->vtbl->clone(frame_if, allocator, &clone_if);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (clone_if == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    *frame_out = (sixel_frame_t *)clone_if;
    return SIXEL_OK;
}

static int
sixel_encoder_frame_get_handoff_shareable(sixel_frame_t const *frame)
{
    SIXELSTATUS status;
    sixel_frame_interface_t *frame_if;
    sixel_frame_timeline_t timeline;

    status = SIXEL_FALSE;
    frame_if = NULL;
    memset(&timeline, 0, sizeof(timeline));
    if (frame == NULL) {
        return 0;
    }

    frame_if = sixel_frame_as_interface(frame);
    if (frame_if->vtbl == NULL || frame_if->vtbl->get_timeline == NULL) {
        return 0;
    }

    status = frame_if->vtbl->get_timeline(frame_if, &timeline);
    if (SIXEL_FAILED(status)) {
        return 0;
    }

    return timeline.handoff_shareable != 0 ? 1 : 0;
}

static int
sixel_encoder_frame_get_transparent_mask_pixels(
    sixel_frame_t const *frame,
    size_t *pixel_count_out)
{
    SIXELSTATUS status;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t transparency;
    size_t pixel_count;

    status = SIXEL_FALSE;
    memset(&view, 0, sizeof(view));
    pixel_count = 0u;
    memset(&transparency, 0, sizeof(transparency));
    if (pixel_count_out != NULL) {
        *pixel_count_out = 0u;
    }
    if (frame == NULL) {
        return 0;
    }
    status = sixel_encoder_frame_get_transparency(frame, &transparency);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    if (transparency.transparent_mask == NULL ||
        transparency.transparent_mask_size == 0u) {
        return 0;
    }
    status = sixel_encoder_frame_get_pixels_view(frame, &view);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    if (view.width <= 0 || view.height <= 0) {
        return 0;
    }
    if ((size_t)view.width > SIZE_MAX / (size_t)view.height) {
        return 0;
    }
    pixel_count = (size_t)view.width * (size_t)view.height;
    if (transparency.transparent_mask_size < pixel_count) {
        return 0;
    }
    if (pixel_count_out != NULL) {
        *pixel_count_out = pixel_count;
    }

    return 1;
}

static int
sixel_encoder_frame_has_transparent_mask(sixel_frame_t const *frame)
{
    return sixel_encoder_frame_get_transparent_mask_pixels(frame, NULL);
}

static int
sixel_encoder_frame_preserves_alpha_key(sixel_frame_t const *frame)
{
    SIXELSTATUS status;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t transparency;

    status = SIXEL_FALSE;
    memset(&view, 0, sizeof(view));
    memset(&transparency, 0, sizeof(transparency));

    if (frame == NULL) {
        return 0;
    }
    status = sixel_encoder_frame_get_transparency(frame, &transparency);
    if (SIXEL_FAILED(status) ||
        transparency.alpha_zero_is_transparent == 0) {
        return 0;
    }
    status = sixel_encoder_frame_get_pixels_view(frame, &view);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    if (sixel_encoder_pixelformat_has_alpha(view.pixelformat)
            || sixel_encoder_frame_has_transparent_mask(frame)) {
        return 1;
    }

    return 0;
}

static void
sixel_encoder_bind_frame_transparent_mask(
    sixel_dither_t *dither,
    sixel_frame_t const *frame)
{
    SIXELSTATUS status;
    sixel_frame_transparency_t transparency;
    size_t pixel_count;

    status = SIXEL_FALSE;
    memset(&transparency, 0, sizeof(transparency));
    pixel_count = 0u;
    if (dither == NULL) {
        return;
    }
    sixel_dither_clear_pipeline_transparent_mask_hint(dither);

    /*
     * Reuse frame-owned transparency masks directly in the dither pipeline.
     * This keeps the source precision intact by avoiding temporary RGBA
     * conversion while still enabling keycolor-based transparent output.
     */
    if (frame == NULL || dither->keycolor < 0
            || dither->keycolor >= SIXEL_PALETTE_MAX) {
        return;
    }
    if (!sixel_encoder_frame_get_transparent_mask_pixels(frame,
                                                         &pixel_count)) {
        return;
    }
    status = sixel_encoder_frame_get_transparency(frame, &transparency);
    if (SIXEL_FAILED(status)) {
        return;
    }

    sixel_dither_set_pipeline_transparent_mask_hint(
        dither,
        transparency.transparent_mask,
        pixel_count,
        dither->keycolor);
}

/*
 * Promote PAL8 frames with transparent key indices before geometry filters.
 * Clip/resize work on RGB payloads, so the transparent key is converted to a
 * side mask to keep transparency aligned with transformed pixels.
 */
static SIXELSTATUS
sixel_encoder_promote_pal8_transparent_for_geometry(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame)
{
    SIXELSTATUS status;
    unsigned char *indices;
    unsigned char *mask;
    sixel_allocator_t *allocator;
    sixel_frame_transparency_t transparency;
    size_t pixel_count;
    size_t index;
    int clip_active;
    int scale_active;
    int transparent_index;
    int ncolors;
    int has_transparent;
    unsigned char palette_index;

    status = SIXEL_OK;
    indices = NULL;
    mask = NULL;
    allocator = NULL;
    memset(&transparency, 0, sizeof(transparency));
    pixel_count = 0u;
    index = 0u;
    clip_active = 0;
    scale_active = 0;
    transparent_index = -1;
    ncolors = 0;
    has_transparent = 0;
    palette_index = 0u;

    if (encoder == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    allocator = encoder->allocator;
    if (allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    clip_active = (encoder->clipwidth > 0 && encoder->clipheight > 0)
        ? 1
        : 0;
    scale_active = (encoder->pixelwidth >= 0
                    || encoder->pixelheight >= 0
                    || encoder->percentwidth >= 0
                    || encoder->percentheight >= 0)
        ? 1
        : 0;
    if (clip_active == 0 && scale_active == 0) {
        return SIXEL_OK;
    }

    if (sixel_frame_get_pixelformat(frame) != SIXEL_PIXELFORMAT_PAL8) {
        return SIXEL_OK;
    }

    transparent_index = sixel_frame_get_transparent(frame);
    if (transparent_index < 0 || transparent_index >= SIXEL_PALETTE_MAX) {
        return SIXEL_OK;
    }

    if (sixel_frame_get_width(frame) <= 0 ||
        sixel_frame_get_height(frame) <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)sixel_frame_get_width(frame)
        > SIZE_MAX / (size_t)sixel_frame_get_height(frame)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)sixel_frame_get_width(frame) *
                  (size_t)sixel_frame_get_height(frame);

    indices = sixel_frame_get_pixels(frame);
    if (indices == NULL || sixel_frame_get_palette(frame) == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_promote_pal8_transparent_for_geometry: "
            "indexed frame payload is missing.");
        return SIXEL_BAD_ARGUMENT;
    }

    mask = (unsigned char *)sixel_allocator_malloc(allocator, pixel_count);
    if (mask == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_promote_pal8_transparent_for_geometry: "
            "sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    ncolors = sixel_frame_get_ncolors(frame);
    for (index = 0u; index < pixel_count; ++index) {
        palette_index = indices[index];
        if (ncolors > 0 && (int)palette_index >= ncolors) {
            sixel_helper_set_additional_message(
                "sixel_encoder_promote_pal8_transparent_for_geometry: "
                "palette index out of range.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if ((int)palette_index == transparent_index) {
            mask[index] = 1u;
            has_transparent = 1;
        } else {
            mask[index] = 0u;
        }
    }

    status = sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_RGB888);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    transparency.transparent = (-1);
    if (has_transparent != 0) {
        transparency.transparent_mask = mask;
        transparency.transparent_mask_size = pixel_count;
        transparency.alpha_zero_is_transparent = 1;
        mask = NULL;
    } else {
        transparency.alpha_zero_is_transparent = 0;
    }
    status = sixel_encoder_frame_set_transparency(frame, &transparency);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_frame_set_ncolors(frame, (-1));

end:
    sixel_allocator_free(allocator, mask);
    return status;
}

/*
 * Convert frame pixels into the requested colorspace without changing
 * the current pixelformat.
 */
static SIXELSTATUS
sixel_encoder_convert_frame_colorspace(sixel_frame_t *frame,
                                       int target_colorspace)
{
    SIXELSTATUS status;
    int source_colorspace;
    int pixelformat;
    int depth;
    int width;
    int height;
    size_t pixel_total;
    size_t pixel_bytes;
    unsigned char *pixels;
    sixel_frame_pixels_view_t view;

    status = SIXEL_BAD_ARGUMENT;
    source_colorspace = SIXEL_COLORSPACE_GAMMA;
    pixelformat = SIXEL_PIXELFORMAT_RGB888;
    depth = 0;
    width = 0;
    height = 0;
    pixel_total = 0U;
    pixel_bytes = 0U;
    pixels = NULL;
    memset(&view, 0, sizeof(view));

    if (frame == NULL) {
        return status;
    }

    source_colorspace = sixel_frame_get_colorspace(frame);
    if (source_colorspace == target_colorspace) {
        return SIXEL_OK;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    if (width <= 0 || height <= 0) {
        return SIXEL_BAD_INPUT;
    }

    pixelformat = sixel_frame_get_pixelformat(frame);
    depth = sixel_helper_compute_depth(pixelformat);
    if (depth <= 0) {
        return SIXEL_BAD_INPUT;
    }

    pixel_total = (size_t)width * (size_t)height;
    if (pixel_total / (size_t)width != (size_t)height) {
        return SIXEL_BAD_INPUT;
    }
    if (pixel_total > SIZE_MAX / (size_t)depth) {
        return SIXEL_BAD_INPUT;
    }
    pixel_bytes = pixel_total * (size_t)depth;

    status = sixel_encoder_frame_get_pixels_view(frame, &view);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    pixels = SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)
        ? (unsigned char *)view.pixels_float32
        : view.pixels;

    status = sixel_helper_convert_colorspace(pixels,
                                             pixel_bytes,
                                             pixelformat,
                                             source_colorspace,
                                             target_colorspace);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    sixel_frame_set_colorspace(frame, target_colorspace);
    return SIXEL_OK;
}

/*
 * Recolor the generated palette into the working colorspace so dithering,
 * LUT builders, and palette emission share the same color interpretation.
 */
static SIXELSTATUS
sixel_encoder_convert_palette_colorspace(sixel_palette_t *palette,
                                         int source_colorspace,
                                         int target_colorspace)
{
    SIXELSTATUS status;
    size_t palette_count;
    size_t palette_channels;
    size_t palette_bytes;
    size_t palette_float_bytes;
    size_t index;
    int channel;
    int float_pixelformat;
    int source_pixelformat;
    sixel_palette_entries_view_t entries_view;
    sixel_palette_float32_entries_view_t float32_view;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (source_colorspace == target_colorspace) {
        return SIXEL_OK;
    }

    memset(&entries_view, 0, sizeof(entries_view));
    memset(&float32_view, 0, sizeof(float32_view));
    if (palette->vtbl == NULL || palette->vtbl->get_entries == NULL ||
        palette->vtbl->get_entries_float32 == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = palette->vtbl->get_entries(palette, &entries_view);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = palette->vtbl->get_entries_float32(palette, &float32_view);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    palette_count = entries_view.entry_count;
    if (palette_count == 0U) {
        palette_count = float32_view.entry_count;
    }
    if (palette_count == 0U) {
        return SIXEL_OK;
    }

    palette_channels = palette_count * 3U;
    if (palette_channels / 3U != palette_count) {
        return SIXEL_BAD_INPUT;
    }

    if (float32_view.entries != NULL && float32_view.depth > 0) {
        palette_float_bytes =
            palette_count * (size_t)float32_view.depth;
        if ((size_t)float32_view.depth == 0U
                || palette_float_bytes / (size_t)float32_view.depth
                    != palette_count) {
            return SIXEL_BAD_INPUT;
        }
        source_pixelformat =
            sixel_encoder_pixelformat_for_colorspace(source_colorspace, 1);
        status = sixel_helper_convert_colorspace(
            (unsigned char *)float32_view.entries,
            palette_float_bytes,
            source_pixelformat,
            source_colorspace,
            target_colorspace);
        if (SIXEL_FAILED(status)) {
            return status;
        }

        if (entries_view.entries == NULL || entries_view.depth < 3) {
            return SIXEL_OK;
        }

        float_pixelformat =
            sixel_encoder_pixelformat_for_colorspace(target_colorspace, 1);
        for (index = 0U; index < palette_channels; ++index) {
            channel = (int)(index % 3U);
            entries_view.entries[index] =
                sixel_pixelformat_float_channel_to_byte(
                    float_pixelformat,
                    channel,
                    float32_view.entries[index]);
        }

        return SIXEL_OK;
    }

    if (entries_view.entries == NULL || entries_view.depth < 3) {
        return SIXEL_OK;
    }

    palette_bytes = palette_channels;
    status = sixel_helper_convert_colorspace(entries_view.entries,
                                             palette_bytes,
                                             SIXEL_PIXELFORMAT_RGB888,
                                             source_colorspace,
                                             target_colorspace);
    return status;
}

static int
sixel_encoder_try_parse_toggle01_text(char const *text, int *value_out)
{
    char *endptr;
    long parsed;

    endptr = NULL;
    parsed = 0L;
    if (text == NULL || value_out == NULL) {
        return 0;
    }

    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (endptr == text || endptr == NULL || endptr[0] != '\0'
            || errno == ERANGE || (parsed != 0L && parsed != 1L)) {
        return 0;
    }

    *value_out = (int)parsed;
    return 1;
}

static int
sixel_encoder_try_parse_ratio01_text(char const *text, double *value_out)
{
    char *endptr;
    double parsed;

    endptr = NULL;
    parsed = 0.0;
    if (text == NULL || value_out == NULL) {
        return 0;
    }

    errno = 0;
    parsed = strtod(text, &endptr);
    if (endptr == text || endptr == NULL || endptr[0] != '\0'
            || errno != 0 || parsed != parsed
            || parsed < 0.0 || parsed > 1.0) {
        return 0;
    }

    *value_out = parsed;
    return 1;
}

static void
sixel_encoder_reset_quantize_animation_state(sixel_encoder_t *encoder)
{
    if (encoder == NULL) {
        return;
    }

    encoder->quantize_animation_prev_palette_count = 0U;
    encoder->quantize_animation_prev_palette_valid = 0;
    encoder->quantize_animation_prev_palette_float_valid = 0;
    encoder->quantize_animation_prev_palette_float_stride = 0;
    encoder->quantize_animation_prev_probe_valid = 0;
    encoder->quantize_animation_prev_width = 0;
    encoder->quantize_animation_prev_height = 0;
}

static void
sixel_encoder_resolve_quantize_animation_options(
    sixel_encoder_t const *encoder,
    int *animation_mode_out,
    double *scene_cut_threshold_out)
{
    char const *env_value;
    int resolved_mode;
    double resolved_threshold;

    env_value = NULL;
    resolved_mode = 0;
    resolved_threshold = SIXEL_QUANTIZE_SCENE_CUT_THRESHOLD_DEFAULT;
    if (encoder == NULL || animation_mode_out == NULL
            || scene_cut_threshold_out == NULL) {
        return;
    }

    if (encoder->quantize_model_animation_mode_override != 0) {
        resolved_mode = encoder->quantize_model_animation_mode;
    } else {
        env_value = sixel_compat_getenv(SIXEL_PALETTE_ANIMATION_MODE_ENVVAR);
        if (!sixel_encoder_try_parse_toggle01_text(env_value,
                                                   &resolved_mode)) {
            resolved_mode = 0;
        }
    }

    if (encoder->quantize_model_scene_cut_threshold_override != 0) {
        resolved_threshold = encoder->quantize_model_scene_cut_threshold;
    } else {
        env_value = sixel_compat_getenv(
            SIXEL_PALETTE_SCENE_CUT_THRESHOLD_ENVVAR);
        if (!sixel_encoder_try_parse_ratio01_text(env_value,
                                                  &resolved_threshold)) {
            resolved_threshold = SIXEL_QUANTIZE_SCENE_CUT_THRESHOLD_DEFAULT;
        }
    }

    if (resolved_threshold < 0.0 || resolved_threshold > 1.0) {
        resolved_threshold = SIXEL_QUANTIZE_SCENE_CUT_THRESHOLD_DEFAULT;
    }

    *animation_mode_out = resolved_mode;
    *scene_cut_threshold_out = resolved_threshold;
}

static int
sixel_encoder_quantize_animation_enabled_for_frame(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame)
{
    int animation_mode;
    double scene_cut_threshold;
    int frame_no;
    int loop_no;
    int multiframe;

    animation_mode = 0;
    scene_cut_threshold = SIXEL_QUANTIZE_SCENE_CUT_THRESHOLD_DEFAULT;
    frame_no = 0;
    loop_no = 0;
    multiframe = 0;
    if (encoder == NULL || frame == NULL) {
        return 0;
    }

    sixel_encoder_resolve_quantize_animation_options(
        encoder,
        &animation_mode,
        &scene_cut_threshold);
    if (animation_mode == 0) {
        return 0;
    }

    frame_no = sixel_frame_get_frame_no(frame);
    loop_no = sixel_frame_get_loop_no(frame);
    multiframe = sixel_frame_get_multiframe(frame);
    if (multiframe == 0 && frame_no == 0 && loop_no == 0) {
        return 0;
    }

    return 1;
}

static SIXELSTATUS
sixel_encoder_collect_scene_probe(sixel_frame_t *frame,
                                  sixel_allocator_t *allocator,
                                  unsigned char probe_out[])
{
    SIXELSTATUS status;
    unsigned char *normalized;
    unsigned char *source_pixels;
    size_t normalized_bytes;
    int normalized_pixelformat;
    int width;
    int height;
    int pixelformat;
    int sample_x;
    int sample_y;
    int x;
    int y;
    size_t probe_index;
    size_t pixel_index;
    sixel_frame_pixels_view_t view;

    status = SIXEL_OK;
    normalized = NULL;
    source_pixels = NULL;
    normalized_bytes = 0U;
    normalized_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    width = 0;
    height = 0;
    pixelformat = SIXEL_PIXELFORMAT_RGB888;
    sample_x = 0;
    sample_y = 0;
    x = 0;
    y = 0;
    probe_index = 0U;
    pixel_index = 0U;
    memset(&view, 0, sizeof(view));
    if (frame == NULL || allocator == NULL || probe_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    if (width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    pixelformat = sixel_frame_get_pixelformat(frame);
    status = sixel_encoder_frame_get_pixels_view(frame, &view);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    source_pixels = SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)
        ? (unsigned char *)view.pixels_float32
        : view.pixels;
    if (source_pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    normalized_pixelformat = pixelformat;
    if (pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        if ((size_t)width > SIZE_MAX / (size_t)height
                || (size_t)width * (size_t)height > SIZE_MAX / 3U) {
            return SIXEL_BAD_INPUT;
        }
        normalized_bytes = (size_t)width * (size_t)height * 3U;
        normalized = (unsigned char *)sixel_allocator_malloc(
            allocator,
            normalized_bytes);
        if (normalized == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        status = sixel_helper_normalize_pixelformat(
            normalized,
            &normalized_pixelformat,
            source_pixels,
            pixelformat,
            width,
            height);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(allocator, normalized);
            return status;
        }
        source_pixels = normalized;
    }

    if (normalized_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        if (normalized != NULL && allocator != NULL) {
            sixel_allocator_free(allocator, normalized);
        }
        return SIXEL_BAD_INPUT;
    }

    probe_index = 0U;
    for (sample_y = 0; sample_y < SIXEL_QUANTIZE_SCENE_PROBE_GRID_SIDE;
            ++sample_y) {
        if (height <= 1) {
            y = 0;
        } else {
            y = sample_y * (height - 1)
                / (SIXEL_QUANTIZE_SCENE_PROBE_GRID_SIDE - 1);
        }
        for (sample_x = 0; sample_x < SIXEL_QUANTIZE_SCENE_PROBE_GRID_SIDE;
                ++sample_x) {
            if (width <= 1) {
                x = 0;
            } else {
                x = sample_x * (width - 1)
                    / (SIXEL_QUANTIZE_SCENE_PROBE_GRID_SIDE - 1);
            }
            pixel_index = ((size_t)y * (size_t)width + (size_t)x) * 3U;
            probe_out[probe_index + 0U] = source_pixels[pixel_index + 0U];
            probe_out[probe_index + 1U] = source_pixels[pixel_index + 1U];
            probe_out[probe_index + 2U] = source_pixels[pixel_index + 2U];
            probe_index += 3U;
        }
    }

    if (normalized != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, normalized);
    }
    return SIXEL_OK;
}

static double
sixel_encoder_scene_probe_distance(unsigned char const *left,
                                   unsigned char const *right)
{
    size_t index;
    double diff_sum;
    int delta;

    index = 0U;
    diff_sum = 0.0;
    delta = 0;
    if (left == NULL || right == NULL) {
        return 1.0;
    }

    while (index < SIXEL_QUANTIZE_SCENE_PROBE_BYTES) {
        delta = (int)left[index] - (int)right[index];
        if (delta < 0) {
            delta = -delta;
        }
        diff_sum += (double)delta;
        ++index;
    }

    return diff_sum / (1020.0 * (double)SIXEL_QUANTIZE_SCENE_PROBE_BYTES);
}

static void
sixel_encoder_restore_previous_palette(
    sixel_palette_t *palette,
    unsigned char const *prev_palette,
    float const *prev_palette_float,
    unsigned int prev_count,
    int prev_float_valid,
    int prev_float_stride)
{
    size_t entry_bytes;
    size_t float_bytes;
    int float_stride;
    unsigned int color_count;
    sixel_palette_entries_request_t entries_request;
    sixel_palette_entries_view_t entries_view;
    sixel_palette_float32_entries_request_t float32_request;
    sixel_palette_float32_entries_view_t float32_view;

    entry_bytes = 0u;
    float_bytes = 0u;
    float_stride = 0;
    memset(&entries_request, 0, sizeof(entries_request));
    memset(&entries_view, 0, sizeof(entries_view));
    memset(&float32_request, 0, sizeof(float32_request));
    memset(&float32_view, 0, sizeof(float32_view));
    if (palette == NULL || prev_palette == NULL || prev_count == 0U
            || palette->vtbl == NULL || palette->vtbl->get_entries == NULL
            || palette->vtbl->get_entries_float32 == NULL
            || palette->vtbl->init_entries == NULL
            || palette->vtbl->init_entries_float32 == NULL) {
        return;
    }
    if (SIXEL_FAILED(palette->vtbl->get_entries(palette, &entries_view))
            || entries_view.entries == NULL || entries_view.depth != 3) {
        return;
    }

    color_count = prev_count;
    if (color_count > (unsigned int)SIXEL_PALETTE_MAX) {
        color_count = (unsigned int)SIXEL_PALETTE_MAX;
    }
    entry_bytes = (size_t)color_count * 3u;
    if (entry_bytes > entries_view.entries_size) {
        return;
    }
    entries_request.entries = prev_palette;
    entries_request.colors = color_count;
    entries_request.depth = 3;
    if (SIXEL_FAILED(palette->vtbl->init_entries(palette, &entries_request))) {
        return;
    }

    if (SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                                                        &float32_view))
            || float32_view.entries == NULL || float32_view.depth <= 0) {
        return;
    }
    float_stride = float32_view.depth / (int)sizeof(float);
    if (float_stride <= 0 || (unsigned int)float_stride > SIXEL_MAX_CHANNELS) {
        return;
    }
    if (prev_float_valid == 0 || prev_palette_float == NULL
            || prev_float_stride != float_stride) {
        return;
    }
    float_bytes = (size_t)color_count * (size_t)float_stride * sizeof(float);
    if (float_bytes > float32_view.entries_size) {
        return;
    }
    float32_request.entries = prev_palette_float;
    float32_request.colors = color_count;
    float32_request.depth = float32_view.depth;
    (void)palette->vtbl->init_entries_float32(palette, &float32_request);
}

static SIXELSTATUS
sixel_encoder_apply_quantize_animation_mode(sixel_encoder_t *encoder,
                                            sixel_frame_t *frame,
                                            sixel_dither_t *dither)
{
    SIXELSTATUS status;
    sixel_palette_t *palette;
    unsigned int palette_count;
    int animation_mode;
    double scene_cut_threshold;
    unsigned char current_probe[SIXEL_QUANTIZE_SCENE_PROBE_BYTES];
    double scene_score;
    int scene_cut;
    int width;
    int height;
    int frame_no;
    int loop_no;
    int multiframe;
    int current_float_stride;
    int current_float_valid;
    size_t current_float_bytes;
    sixel_palette_entries_view_t entries_view;
    sixel_palette_float32_entries_view_t float32_view;
    sixel_palette_metadata_t metadata;

    status = SIXEL_OK;
    palette = NULL;
    palette_count = 0U;
    animation_mode = 0;
    scene_cut_threshold = SIXEL_QUANTIZE_SCENE_CUT_THRESHOLD_DEFAULT;
    memset(current_probe, 0, sizeof(current_probe));
    scene_score = 0.0;
    scene_cut = 0;
    width = 0;
    height = 0;
    frame_no = 0;
    loop_no = 0;
    multiframe = 0;
    current_float_stride = 0;
    current_float_valid = 0;
    current_float_bytes = 0u;
    memset(&entries_view, 0, sizeof(entries_view));
    memset(&float32_view, 0, sizeof(float32_view));
    memset(&metadata, 0, sizeof(metadata));
    if (encoder == NULL || frame == NULL || dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_encoder_resolve_quantize_animation_options(
        encoder,
        &animation_mode,
        &scene_cut_threshold);
    if (animation_mode == 0) {
        sixel_encoder_reset_quantize_animation_state(encoder);
        return SIXEL_OK;
    }
    frame_no = sixel_frame_get_frame_no(frame);
    loop_no = sixel_frame_get_loop_no(frame);
    multiframe = sixel_frame_get_multiframe(frame);
    if (multiframe == 0 && frame_no == 0 && loop_no == 0) {
        sixel_encoder_reset_quantize_animation_state(encoder);
        return SIXEL_OK;
    }

    palette = dither->palette;
    if (palette == NULL || palette->vtbl == NULL ||
        palette->vtbl->get_entries == NULL ||
        palette->vtbl->get_entries_float32 == NULL ||
        palette->vtbl->get_metadata == NULL ||
        SIXEL_FAILED(palette->vtbl->get_entries(palette, &entries_view)) ||
        entries_view.entries == NULL || entries_view.depth < 3) {
        sixel_encoder_reset_quantize_animation_state(encoder);
        return SIXEL_OK;
    }

    status = sixel_encoder_collect_scene_probe(frame,
                                               encoder->allocator,
                                               current_probe);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    scene_cut = 0;
    if (encoder->quantize_animation_prev_probe_valid == 0
            || encoder->quantize_animation_prev_palette_valid == 0) {
        scene_cut = 1;
    } else if (encoder->quantize_animation_prev_width != width
            || encoder->quantize_animation_prev_height != height) {
        scene_cut = 1;
    } else {
        scene_score = sixel_encoder_scene_probe_distance(
            current_probe,
            encoder->quantize_animation_prev_probe);
        if (scene_score > scene_cut_threshold) {
            scene_cut = 1;
        }
    }

    if (scene_cut == 0) {
        /* Keep palette stable until a scene cut is detected. */
        sixel_encoder_restore_previous_palette(
            palette,
            encoder->quantize_animation_prev_palette,
            encoder->quantize_animation_prev_palette_float,
            encoder->quantize_animation_prev_palette_count,
            encoder->quantize_animation_prev_palette_float_valid,
            encoder->quantize_animation_prev_palette_float_stride);
    }

    memset(&entries_view, 0, sizeof(entries_view));
    memset(&float32_view, 0, sizeof(float32_view));
    memset(&metadata, 0, sizeof(metadata));
    if (SIXEL_FAILED(palette->vtbl->get_entries(palette, &entries_view)) ||
        SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                                                        &float32_view)) ||
        SIXEL_FAILED(palette->vtbl->get_metadata(palette, &metadata))) {
        return SIXEL_OK;
    }
    palette_count = metadata.entry_count;
    if (palette_count > (unsigned int)SIXEL_PALETTE_MAX) {
        palette_count = (unsigned int)SIXEL_PALETTE_MAX;
    }
    if (palette_count == 0U) {
        encoder->quantize_animation_prev_palette_count = 0U;
        encoder->quantize_animation_prev_palette_valid = 0;
        encoder->quantize_animation_prev_palette_float_valid = 0;
        encoder->quantize_animation_prev_palette_float_stride = 0;
    } else {
        if (entries_view.entries == NULL || entries_view.depth < 3 ||
                entries_view.entries_size < (size_t)palette_count * 3U) {
            return SIXEL_OK;
        }
        memcpy(encoder->quantize_animation_prev_palette,
               entries_view.entries,
               (size_t)palette_count * 3U);
        encoder->quantize_animation_prev_palette_count = palette_count;
        encoder->quantize_animation_prev_palette_valid = 1;
        current_float_valid = 0;
        current_float_stride = 0;
        current_float_bytes = 0u;
        if (float32_view.entries != NULL && float32_view.depth > 0) {
            current_float_stride = float32_view.depth / (int)sizeof(float);
            if (current_float_stride > 0
                    && (unsigned int)current_float_stride
                        <= SIXEL_MAX_CHANNELS) {
                current_float_bytes = (size_t)palette_count
                    * (size_t)current_float_stride * sizeof(float);
                if (current_float_bytes <= sizeof(
                        encoder->quantize_animation_prev_palette_float)) {
                    memcpy(encoder->quantize_animation_prev_palette_float,
                           float32_view.entries,
                           current_float_bytes);
                    current_float_valid = 1;
                }
            }
        }
        encoder->quantize_animation_prev_palette_float_valid =
            current_float_valid;
        encoder->quantize_animation_prev_palette_float_stride =
            current_float_stride;
    }
    memcpy(encoder->quantize_animation_prev_probe,
           current_probe,
           sizeof(current_probe));
    encoder->quantize_animation_prev_probe_valid = 1;
    encoder->quantize_animation_prev_width = width;
    encoder->quantize_animation_prev_height = height;

    return SIXEL_OK;
}

static int
sixel_encoder_env_prefers_float32(char const *text)
{
    char lowered[8];
    size_t i;

    if (text == NULL || *text == '\0') {
        return 0;
    }

    for (i = 0; i < sizeof(lowered) - 1 && text[i] != '\0'; ++i) {
        lowered[i] = (char)tolower((unsigned char)text[i]);
    }
    lowered[i] = '\0';

    if (strcmp(lowered, "0") == 0
        || strcmp(lowered, "off") == 0
        || strcmp(lowered, "false") == 0
        || strcmp(lowered, "no") == 0) {
        return 0;
    }

    return 1;
}

static SIXELSTATUS
sixel_encoder_apply_precision_override(
    sixel_encoder_t *encoder,
    sixel_encoder_precision_mode_t mode)
{
    int prefer_float32;

    prefer_float32 = encoder->prefer_float32;
    if (encoder->force_float32_colorspace != 0) {
        prefer_float32 = 1;
    }

    if (mode == SIXEL_ENCODER_PRECISION_MODE_AUTO) {
        return SIXEL_OK;
    }

    if (mode == SIXEL_ENCODER_PRECISION_MODE_FLOAT32) {
        prefer_float32 = 1;
    } else if (mode == SIXEL_ENCODER_PRECISION_MODE_8BIT) {
        if (encoder->force_float32_colorspace != 0) {
            prefer_float32 = 1;
        } else {
            prefer_float32 = 0;
        }
    } else {
        sixel_helper_set_additional_message(
            "sixel_encoder_setopt: invalid precision override.");
        return SIXEL_BAD_ARGUMENT;
    }

    encoder->prefer_float32 = prefer_float32;

    return SIXEL_OK;
}


/*
 * Parse background colors with the same grammar used for OSC11 responses.
 * The encoder keeps ownership of the parsed color via allocator memory.
 */
static SIXELSTATUS
sixel_parse_x_colorspec(
    unsigned char       /* out */ **bgcolor,     /* destination buffer */
    char const          /* in */  *s,            /* source buffer */
    sixel_allocator_t   /* in */  *allocator)    /* allocator object for
                                                    destination buffer */
{
    SIXELSTATUS status;
    unsigned char parsed[3];
    unsigned char *allocated;

    status = SIXEL_FALSE;
    parsed[0] = 0u;
    parsed[1] = 0u;
    parsed[2] = 0u;
    allocated = NULL;

    if (bgcolor == NULL || s == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_tty_parse_colorspec(parsed, s);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    allocated = (unsigned char *)sixel_allocator_malloc(allocator, 3u);
    if (allocated == NULL) {
        sixel_helper_set_additional_message(
            "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    allocated[0] = parsed[0];
    allocated[1] = parsed[1];
    allocated[2] = parsed[2];
    *bgcolor = allocated;

    return SIXEL_OK;
}


static void
sixel_encoder_filter_plan_init(sixel_filter_plan_t *plan)
{
    int index;

    if (plan == NULL) {
        return;
    }

    plan->count = 0;
    for (index = 0; index < SIXEL_ENCODER_FILTER_PLAN_MAX; ++index) {
        plan->nodes[index].filter = NULL;
        plan->nodes[index].kind = SIXEL_FILTER_KIND_GENERIC;
    }
}


static void
sixel_encoder_filter_plan_teardown(sixel_filter_plan_t *plan)
{
    int index;

    if (plan == NULL) {
        return;
    }

    for (index = 0; index < plan->count; ++index) {
        if (plan->nodes[index].filter != NULL) {
            sixel_filter_free(plan->nodes[index].filter);
            plan->nodes[index].filter = NULL;
        }
        plan->nodes[index].kind = SIXEL_FILTER_KIND_GENERIC;
    }
    plan->count = 0;
}


static SIXELSTATUS
sixel_encoder_filter_plan_append(
    sixel_filter_plan_t *plan,
    sixel_filter_kind_t kind,
    const void *config,
    sixel_frame_t **slot,
    int input_pixelformat,
    int input_colorspace,
    int output_pixelformat,
    int output_colorspace,
    int total_units)
{
    SIXELSTATUS status;
    sixel_filter_t *filter;

    status = SIXEL_FALSE;
    filter = NULL;

    if (plan == NULL || slot == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (plan->count >= SIXEL_ENCODER_FILTER_PLAN_MAX) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_filter_factory_create_by_kind(kind, config, &filter);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    sixel_filter_bind_input(filter,
                            slot,
                            input_pixelformat,
                            input_colorspace);
    sixel_filter_bind_output(filter,
                             slot,
                             output_pixelformat,
                             output_colorspace);

    if (total_units > 0) {
        sixel_filter_set_progress(filter, NULL, NULL, total_units);
    }

    plan->nodes[plan->count].filter = filter;
    plan->nodes[plan->count].kind = filter->kind;
    plan->count++;

    return SIXEL_OK;
}


static SIXELSTATUS
sixel_encoder_filter_plan_run(sixel_filter_plan_t *plan,
                              sixel_allocator_t *allocator,
                              sixel_timeline_logger_t *logger)
{
    SIXELSTATUS status;
    int index;

    status = SIXEL_FALSE;

    if (plan == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (index = 0; index < plan->count; ++index) {
        status = sixel_filter_run(plan->nodes[index].filter,
                                  allocator,
                                  logger);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return SIXEL_OK;
}


/* generic writer function for output writer initialization */
static int
sixel_write_callback(char *data, int size, void *priv)
{
    int result;

    result = (int)sixel_compat_write(*(int *)priv,
                                     data,
                                     (size_t)size);

    return result;
}

static int
sixel_encoder_core_write_callback(char *data, int size, void *priv)
{
    sixel_output_t *output;
    SIXELSTATUS status;

    output = (sixel_output_t *)priv;
    if (output == NULL) {
#if HAVE_ERRNO_H
        errno = EINVAL;
#endif
        return (-1);
    }

    status = sixel_output_write_bytes(output, data, size);
    if (SIXEL_FAILED(status)) {
#if HAVE_ERRNO_H
        if ((status & SIXEL_LIBC_ERROR) == SIXEL_LIBC_ERROR &&
            (status & 0xff) != 0) {
            errno = (status & 0xff);
        } else {
            errno = EINVAL;
        }
#endif
        return (-1);
    }

    return size;
}


/* the writer function with hex-encoding for output writer initialization */
static int
sixel_hex_write_callback(
    char    /* in */ *data,
    int     /* in */ size,
    void    /* in */ *priv)
{
    char hex[512];
    unsigned char ch;
    int chunk;
    int pos;
    int i;
    int j;
    int result;
    int total;

    total = 0;
    i = 0;
    while (i < size) {
        chunk = size - i;
        if (chunk > (int)(sizeof(hex) / 2)) {
            chunk = (int)(sizeof(hex) / 2);
        }
        pos = 0;
        for (j = 0; j < chunk; ++j) {
            ch = (unsigned char)data[i + j];
            hex[pos] = (char)((ch >> 4) & 0x0f);
            hex[pos] += (char)(hex[pos] < 10 ? '0' : ('a' - 10));
            ++pos;
            hex[pos] = (char)(ch & 0x0f);
            hex[pos] += (char)(hex[pos] < 10 ? '0' : ('a' - 10));
            ++pos;
        }

        result = (int)sixel_compat_write(*(int *)priv,
                                         hex,
                                         (size_t)pos);
        if (result <= 0) {
            return (total > 0 ? total : result);
        }
        total += result;
        if (result < pos) {
            return total;
        }
        i += chunk;
    }

    return total;
}

static void
sixel_encoder_log_stage(sixel_encoder_t *encoder,
                        sixel_frame_t *frame,
                        char const *worker,
                        char const *role,
                        char const *event,
                        char const *fmt,
                        ...)
{
    sixel_timeline_logger_t *logger;
    int job_id;
    int height;
    char message[256];
    va_list args;

    logger = NULL;
    if (encoder != NULL) {
        logger = encoder->logger;
    }
    if (logger == NULL) {
        return;
    }

    job_id = -1;
    height = 0;
    if (frame != NULL) {
        job_id = sixel_frame_get_frame_no(frame);
        height = sixel_frame_get_height(frame);
    }

    message[0] = '\0';
#if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wformat-nonliteral"
# elif defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-nonliteral"
# endif
#endif
    va_start(args, fmt);
    if (fmt != NULL) {
        (void)vsnprintf(message, sizeof(message), fmt, args);
    }
    va_end(args);
#if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
# if defined(__clang__)
#  pragma clang diagnostic pop
# elif defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif

    sixel_timeline_logger_logf(logger,
                      role,
                      worker,
                      event,
                      job_id,
                      -1,
                      0,
                      height,
                      0,
                      height,
                      "%s",
                      message);
}

static SIXELSTATUS
sixel_encoder_ensure_cell_size(sixel_encoder_t *encoder)
{
#if defined(TIOCGWINSZ) && !defined(__EMSCRIPTEN__)
    struct winsize ws;
    int result;
    int fd = 0;

    if (encoder->cell_width > 0 && encoder->cell_height > 0) {
        return SIXEL_OK;
    }

    fd = sixel_compat_open("/dev/tty", O_RDONLY);
    if (fd >= 0) {
        result = ioctl(fd, TIOCGWINSZ, &ws);
        (void)sixel_compat_close(fd);
    } else {
        sixel_helper_set_additional_message(
            "failed to open /dev/tty");
        return (SIXEL_LIBC_ERROR | (errno & 0xff));
    }
    if (result != 0) {
        sixel_helper_set_additional_message(
            "failed to query terminal geometry with ioctl().");
        return (SIXEL_LIBC_ERROR | (errno & 0xff));
    }

    if (ws.ws_col <= 0 || ws.ws_row <= 0 ||
        ws.ws_xpixel <= ws.ws_col || ws.ws_ypixel <= ws.ws_row) {
        sixel_helper_set_additional_message(
            "terminal does not report pixel cell size for drcs option.");
        return SIXEL_BAD_ARGUMENT;
    }

    encoder->cell_width = ws.ws_xpixel / ws.ws_col;
    encoder->cell_height = ws.ws_ypixel / ws.ws_row;
    if (encoder->cell_width <= 0 || encoder->cell_height <= 0) {
        sixel_helper_set_additional_message(
            "terminal cell size reported zero via ioctl().");
        return SIXEL_BAD_ARGUMENT;
    }

    return SIXEL_OK;
#else
    (void) encoder;
    sixel_helper_set_additional_message(
        "drcs option is not supported on this platform.");
    return SIXEL_NOT_IMPLEMENTED;
#endif
}


/* returns monochrome dithering context object */
static SIXELSTATUS
sixel_prepare_monochrome_palette(
    sixel_dither_t  /* out */ **dither,
     int            /* in */  finvert)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (finvert) {
        *dither = sixel_dither_get(SIXEL_BUILTIN_MONO_LIGHT);
    } else {
        *dither = sixel_dither_get(SIXEL_BUILTIN_MONO_DARK);
    }
    if (*dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prepare_monochrome_palette: sixel_dither_get() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}

static void
sixel_encoder_update_dither_frame_context(sixel_dither_t *dither,
                                          int frame_no,
                                          int loop_no,
                                          int multiframe)
{
    if (dither == NULL) {
        return;
    }

    sixel_dither_set_frame_context(dither,
                                   frame_no,
                                   loop_no,
                                   multiframe);
}


static SIXELSTATUS
sixel_encoder_capture_quantized(sixel_encoder_t *encoder,
                                sixel_dither_t *dither,
                                unsigned char const *pixels,
                                size_t size,
                                int width,
                                int height,
                                int pixelformat,
                                int source_colorspace,
                                int colorspace)
{
    SIXELSTATUS status;
    int ncolors;
    size_t palette_bytes;
    unsigned char *new_pixels;
    unsigned char *new_palette;
    size_t capture_bytes;
    unsigned char const *capture_source;
    sixel_index_t *paletted_pixels;
    size_t quantized_pixels;
    sixel_allocator_t *dither_allocator;
    int saved_pixelformat;
    int restore_pixelformat;

    /*
     * Preserve the quantized frame for later inspection or export.
     *
     *     +-----------------+     +---------------------+
     *     | quantized bytes | --> | encoder->capture_*  |
     *     +-----------------+     +---------------------+
     */

    status = SIXEL_OK;
    ncolors = 0;
    palette_bytes = 0;
    new_pixels = NULL;
    new_palette = NULL;
    capture_bytes = size;
    capture_source = pixels;
    paletted_pixels = NULL;
    quantized_pixels = 0;
    dither_allocator = NULL;

    if (encoder == NULL || pixels == NULL ||
            (dither == NULL && size == 0)) {
        sixel_helper_set_additional_message(
            "sixel_encoder_capture_quantized: invalid capture request.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (!encoder->capture_quantized) {
        return SIXEL_OK;
    }

    saved_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    restore_pixelformat = 0;
    if (dither != NULL) {
        dither_allocator = dither->allocator;
        saved_pixelformat = dither->pixelformat;
        restore_pixelformat = 1;
        if (width <= 0 || height <= 0) {
            sixel_helper_set_additional_message(
                "sixel_encoder_capture_quantized: invalid dimensions.");
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }
        quantized_pixels = (size_t)width * (size_t)height;
        if (height != 0 &&
                quantized_pixels / (size_t)height != (size_t)width) {
            sixel_helper_set_additional_message(
                "sixel_encoder_capture_quantized: image too large.");
            status = SIXEL_RUNTIME_ERROR;
            goto cleanup;
        }
        paletted_pixels = sixel_dither_apply_palette_with_mode(
            dither,
            (unsigned char *)pixels,
            width,
            height,
            SIXEL_DITHER_APPLY_PRESERVE_INTERFRAME_STATE);
        if (paletted_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_capture_quantized: palette conversion failed.");
            status = SIXEL_RUNTIME_ERROR;
            goto cleanup;
        }
        capture_source = (unsigned char const *)paletted_pixels;
        capture_bytes = quantized_pixels;
    }

    if (capture_bytes > 0) {
        if (encoder->capture_pixels == NULL ||
                encoder->capture_pixels_size < capture_bytes) {
            new_pixels = (unsigned char *)sixel_allocator_malloc(
                encoder->allocator, capture_bytes);
            if (new_pixels == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_capture_quantized: "
                    "sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto cleanup;
            }
            sixel_allocator_free(encoder->allocator, encoder->capture_pixels);
            encoder->capture_pixels = new_pixels;
            encoder->capture_pixels_size = capture_bytes;
        }
        memcpy(encoder->capture_pixels, capture_source, capture_bytes);
    }
    encoder->capture_pixel_bytes = capture_bytes;

    ncolors = 0;
    palette_bytes = 0;
    if (dither != NULL) {
        sixel_palette_t *palette_obj = NULL;
        sixel_palette_entries_view_t palette_view;
        size_t palette_count = 0U;

        memset(&palette_view, 0, sizeof(palette_view));
        palette_obj = dither->palette;
        if (palette_obj != NULL && palette_obj->vtbl != NULL &&
                palette_obj->vtbl->get_entries != NULL) {
            status = palette_obj->vtbl->get_entries(palette_obj,
                                                    &palette_view);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            if (palette_view.entries != NULL &&
                    palette_view.entry_count > 0U &&
                    palette_view.depth == 3) {
                palette_count = palette_view.entry_count;
                palette_bytes = palette_count * 3U;
                ncolors = (int)palette_count;
                if (encoder->capture_palette == NULL
                        || encoder->capture_palette_size < palette_bytes) {
                    new_palette = (unsigned char *)sixel_allocator_malloc(
                        encoder->allocator, palette_bytes);
                    if (new_palette == NULL) {
                        sixel_helper_set_additional_message(
                            "sixel_encoder_capture_quantized: "
                            "sixel_allocator_malloc() failed.");
                        status = SIXEL_BAD_ALLOCATION;
                        goto cleanup;
                    }
                    sixel_allocator_free(encoder->allocator,
                                         encoder->capture_palette);
                    encoder->capture_palette = new_palette;
                    encoder->capture_palette_size = palette_bytes;
                }
                memcpy(encoder->capture_palette,
                       palette_view.entries,
                       palette_bytes);
                if (source_colorspace != colorspace) {
                    (void)sixel_helper_convert_colorspace(
                        encoder->capture_palette,
                        palette_bytes,
                        SIXEL_PIXELFORMAT_RGB888,
                        source_colorspace,
                        colorspace);
                }
            }
        }
    }

    encoder->capture_width = width;
    encoder->capture_height = height;
    if (dither != NULL) {
        encoder->capture_pixelformat = SIXEL_PIXELFORMAT_PAL8;
    } else {
        encoder->capture_pixelformat = pixelformat;
    }
    encoder->capture_colorspace = colorspace;
    encoder->capture_palette_size = palette_bytes;
    encoder->capture_ncolors = ncolors;
    encoder->capture_valid = 1;

cleanup:
    if (restore_pixelformat && dither != NULL) {
        /*
         * Undo the normalization performed by sixel_dither_apply_palette().
         *
         *     RGBA8888 --capture--> RGB888 (temporary)
         *          \______________________________/
         *                          |
         *                 restore original state for
         *                 the real encoder execution.
         */
        sixel_dither_set_pixelformat(dither, saved_pixelformat);
    }
    if (paletted_pixels != NULL && dither_allocator != NULL) {
        sixel_allocator_free(dither_allocator, paletted_pixels);
    }

    return status;
}

static SIXELSTATUS
sixel_encoder_capture_quantized_palette_only(
    sixel_encoder_t *encoder,
    sixel_dither_t *dither,
    int width,
    int height,
    int colorspace)
{
    SIXELSTATUS status;
    size_t quantized_pixels;
    unsigned char *new_pixels;
    sixel_palette_t *palette_obj;
    sixel_palette_entries_view_t palette_view;
    size_t palette_count;
    size_t palette_bytes;
    unsigned char *new_palette;
    int ncolors;

    status = SIXEL_OK;
    quantized_pixels = 0u;
    new_pixels = NULL;
    palette_obj = NULL;
    memset(&palette_view, 0, sizeof(palette_view));
    palette_count = 0u;
    palette_bytes = 0u;
    new_palette = NULL;
    ncolors = 0;
    if (encoder == NULL || dither == NULL || width <= 0 || height <= 0) {
        sixel_helper_set_additional_message(
            "sixel_encoder_capture_quantized_palette_only: "
            "invalid capture request.");
        return SIXEL_BAD_ARGUMENT;
    }
    quantized_pixels = (size_t)width * (size_t)height;
    if (height != 0 &&
        quantized_pixels / (size_t)height != (size_t)width) {
        sixel_helper_set_additional_message(
            "sixel_encoder_capture_quantized_palette_only: "
            "image too large.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (encoder->capture_pixels == NULL ||
        encoder->capture_pixels_size < quantized_pixels) {
        new_pixels = (unsigned char *)sixel_allocator_malloc(
            encoder->allocator,
            quantized_pixels);
        if (new_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_capture_quantized_palette_only: "
                "sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        sixel_allocator_free(encoder->allocator, encoder->capture_pixels);
        encoder->capture_pixels = new_pixels;
        encoder->capture_pixels_size = quantized_pixels;
    }
    memset(encoder->capture_pixels, 0, quantized_pixels);
    encoder->capture_pixel_bytes = quantized_pixels;

    palette_obj = dither->palette;
    if (palette_obj == NULL || palette_obj->vtbl == NULL ||
            palette_obj->vtbl->get_entries == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_capture_quantized_palette_only: "
            "palette unavailable.");
        return SIXEL_RUNTIME_ERROR;
    }
    status = palette_obj->vtbl->get_entries(palette_obj, &palette_view);
    if (SIXEL_FAILED(status) || palette_view.entries == NULL ||
            palette_view.entry_count == 0u || palette_view.depth != 3) {
        sixel_helper_set_additional_message(
            "sixel_encoder_capture_quantized_palette_only: "
            "failed to copy palette.");
        return SIXEL_RUNTIME_ERROR;
    }
    palette_count = palette_view.entry_count;
    if (palette_count > (size_t)INT_MAX) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    palette_bytes = palette_count * 3u;
    ncolors = (int)palette_count;
    if (encoder->capture_palette == NULL ||
        encoder->capture_palette_size < palette_bytes) {
        new_palette = (unsigned char *)sixel_allocator_malloc(
            encoder->allocator,
            palette_bytes);
        if (new_palette == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_capture_quantized_palette_only: "
                "sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        sixel_allocator_free(encoder->allocator, encoder->capture_palette);
        encoder->capture_palette = new_palette;
        encoder->capture_palette_size = palette_bytes;
    }
    memcpy(encoder->capture_palette, palette_view.entries, palette_bytes);

    encoder->capture_width = width;
    encoder->capture_height = height;
    encoder->capture_pixelformat = SIXEL_PIXELFORMAT_PAL8;
    encoder->capture_colorspace = colorspace;
    encoder->capture_ncolors = ncolors;
    encoder->capture_valid = 1;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_prepare_builtin_palette(
    sixel_dither_t /* out */ **dither,
    int            /* in */  builtin_palette)
{
    SIXELSTATUS status = SIXEL_FALSE;

    *dither = sixel_dither_get(builtin_palette);
    if (*dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prepare_builtin_palette: sixel_dither_get() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}

static void
sixel_encoder_clear_dither_cache(sixel_encoder_t *encoder)
{
    if (encoder == NULL || encoder->dither_cache == NULL) {
        return;
    }

    sixel_dither_unref((sixel_dither_t *)encoder->dither_cache);
    encoder->dither_cache = NULL;
}

static int
sixel_encoder_thumbnail_hint(sixel_encoder_t *encoder)
{
    int width_hint;
    int height_hint;
    long base;
    long size;

    width_hint = 0;
    height_hint = 0;
    base = 0;
    size = 0;

    if (encoder == NULL) {
        return 0;
    }

    width_hint = encoder->pixelwidth;
    height_hint = encoder->pixelheight;

    /* Request extra resolution for downscaling to preserve detail. */
    if (width_hint > 0 && height_hint > 0) {
        /* Follow the CLI rule: double the larger axis before doubling
         * again for the final request size. */
        if (width_hint >= height_hint) {
            base = (long)width_hint;
        } else {
            base = (long)height_hint;
        }
        base *= 2L;
    } else if (width_hint > 0) {
        base = (long)width_hint;
    } else if (height_hint > 0) {
        base = (long)height_hint;
    } else {
        return 0;
    }

    size = base * 2L;
    if (size > (long)INT_MAX) {
        size = (long)INT_MAX;
    }
    if (size < 1L) {
        size = 1L;
    }

    return (int)size;
}


typedef struct sixel_callback_context_for_mapfile {
    int reqcolors;
    sixel_dither_t *dither;
    sixel_allocator_t *allocator;
    int working_colorspace;
    int lut_policy;
    int lut_policy_shared_instance_override;
    int lut_policy_shared_instance;
    int prefer_float32;
} sixel_callback_context_for_mapfile_t;


/* callback function for sixel_helper_load_image_file() */
static SIXELSTATUS
load_image_callback_for_palette(
    sixel_frame_t   /* in */    *frame, /* frame object from image loader */
    void            /* in */    *data)  /* private data */
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_callback_context_for_mapfile_t *callback_context;

    /* get callback context object from the private data */
    callback_context = (sixel_callback_context_for_mapfile_t *)data;

    status = sixel_frame_set_pixelformat(
        frame,
        sixel_encoder_pixelformat_for_colorspace(
            callback_context->working_colorspace,
            callback_context->prefer_float32));
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    switch (sixel_frame_get_pixelformat(frame)) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_PAL8:
        if (sixel_frame_get_palette(frame) == NULL) {
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        /* create new dither object */
        status = sixel_dither_new(
            &callback_context->dither,
            sixel_frame_get_ncolors(frame),
            callback_context->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        sixel_dither_set_lut_policy(callback_context->dither,
                                    callback_context->lut_policy);
        callback_context->dither->lut_policy_shared_instance_override =
            callback_context->lut_policy_shared_instance_override;
        callback_context->dither->lut_policy_shared_instance =
            callback_context->lut_policy_shared_instance;

        /* use palette which is extracted from the image */
        sixel_dither_set_palette(callback_context->dither,
                                 sixel_frame_get_palette(frame));
        /* success */
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G1:
        /* use 1bpp grayscale builtin palette */
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G1);
        /* success */
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G2:
        /* use 2bpp grayscale builtin palette */
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G1);
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G2);
        /* success */
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G4:
        /* use 4bpp grayscale builtin palette */
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G4);
        /* success */
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G8:
        /* use 8bpp grayscale builtin palette */
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G8);
        /* success */
        status = SIXEL_OK;
        break;
    default:
        /* create new dither object */
        status = sixel_dither_new(
            &callback_context->dither,
            callback_context->reqcolors,
            callback_context->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        sixel_dither_set_lut_policy(callback_context->dither,
                                    callback_context->lut_policy);
        callback_context->dither->lut_policy_shared_instance_override =
            callback_context->lut_policy_shared_instance_override;
        callback_context->dither->lut_policy_shared_instance =
            callback_context->lut_policy_shared_instance;

        /* create adaptive palette from given frame object */
        status = sixel_dither_initialize(callback_context->dither,
                                         sixel_frame_get_pixels(frame),
                                         sixel_frame_get_width(frame),
                                         sixel_frame_get_height(frame),
                                         sixel_frame_get_pixelformat(frame),
                                         SIXEL_LARGE_NORM,
                                         SIXEL_REP_CENTER_BOX,
                                         SIXEL_QUALITY_HIGH);
        if (SIXEL_FAILED(status)) {
            sixel_dither_unref(callback_context->dither);
            goto end;
        }

        /* success */
        status = SIXEL_OK;

        break;
    }

end:
    return status;
}


static SIXELSTATUS
sixel_encoder_emit_palette_output(sixel_encoder_t *encoder);

static SIXELSTATUS
sixel_encoder_enable_quantized_capture_internal(
    sixel_encoder_t *encoder,
    int enable);

static SIXELSTATUS
sixel_encoder_copy_quantized_frame_internal(
    sixel_encoder_t *encoder,
    sixel_allocator_t *allocator,
    sixel_frame_t **ppframe);


static int
sixel_encoder_parse_sample_target(char const *text, size_t *value_out)
{
    char *endptr;
    unsigned long long parsed;

    endptr = NULL;
    parsed = 0ull;

    if (text == NULL || value_out == NULL) {
        return 0;
    }

    errno = 0;
    parsed = strtoull(text, &endptr, 10);
    if (errno == ERANGE || parsed == 0ull) {
        return 0;
    }
    if (endptr == text || *endptr != '\0') {
        return 0;
    }

    *value_out = (size_t)parsed;
    if ((unsigned long long)(*value_out) != parsed) {
        return 0;
    }

    return 1;
}

SIXEL_INTERNAL_API int
sixel_encoder_should_hide_animation_cursor(
    int is_multiframe,
    int fstatic,
    int outfd_is_tty,
    char const *env_value)
{
    if (is_multiframe == 0) {
        return 0;
    }
    if (fstatic != 0) {
        return 0;
    }
    if (outfd_is_tty == 0) {
        return 0;
    }
    if (!sixel_tty_is_animation_hide_cursor_enabled(env_value)) {
        return 0;
    }

    return 1;
}


typedef struct sixel_encode_dag_context {
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    SIXELSTATUS status;
    sixel_dither_t *dither;
    int height;
    int is_animation;
    int nwrite;
    sixel_write_function fn_write;
    sixel_write_function write_callback;
    sixel_write_function scroll_callback;
    void *write_priv;
    void *scroll_priv;
    int target_pixelformat;
    sixel_palette_async_job_t palette_job;
    sixel_dither_t *async_dither;
    int palette_job_started;
    int palette_job_initialized;
    int palette_ready;
    sixel_encoding_planner_t *planner;
    int clip_active;
    sixel_filter_plan_t pre_plan;
    sixel_filter_plan_t post_plan;
    sixel_filter_resize_config_t resize_config;
    sixel_filter_clip_config_t clip_config;
    sixel_filter_colors_config_t colors_config;
    sixel_filter_gradient_config_t gradient_config;
    sixel_filter_dither_config_t dither_config;
    int current_pixelformat;
    int current_colorspace;
    int frame_no;
    int loop_no;
    int delay;
    int multiframe;
} sixel_encode_dag_context_t;

/*
 * Simple DAG scheduler for the encode pipeline.
 *
 * Nodes declare dependencies with a bitmask; the runner executes ready nodes
 * in dependency order and allows palette work to overlap with the pre-plan.
 */
typedef SIXELSTATUS (*sixel_encode_dag_run_fn)(
    sixel_encode_dag_context_t *context);

typedef struct sixel_encode_dag_node {
    char const *label;
    unsigned int deps;
    unsigned int done;
    sixel_encode_dag_run_fn run;
} sixel_encode_dag_node_t;

enum sixel_encode_dag_node_id {
    SIXEL_DAG_NODE_LOAD = 0,
    SIXEL_DAG_NODE_PALETTE_LAUNCH,
    SIXEL_DAG_NODE_PREPLAN,
    SIXEL_DAG_NODE_PALETTE_COLLECT,
    SIXEL_DAG_NODE_DITHER_PLAN,
    SIXEL_DAG_NODE_OUTPUT,
    SIXEL_DAG_NODE_COUNT
};

static SIXELSTATUS
sixel_encode_dag_run_nodes(sixel_encode_dag_context_t *context,
                           sixel_encode_dag_node_t *nodes,
                           int node_count)
{
    int index;
    int remaining;
    int progressed;
    unsigned int completed;
    unsigned int satisfied;
    SIXELSTATUS status;

    if (context == NULL || nodes == NULL || node_count <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    completed = 0u;
    remaining = node_count;
    status = SIXEL_OK;

    while (remaining > 0) {
        progressed = 0;
        for (index = 0; index < node_count; ++index) {
            if (nodes[index].done != 0) {
                continue;
            }
            satisfied = (completed & nodes[index].deps);
            if (satisfied != nodes[index].deps) {
                continue;
            }
            status = nodes[index].run(context);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            nodes[index].done = 1;
            completed |= (1u << index);
            progressed = 1;
            --remaining;
        }
        if (progressed == 0) {
            return SIXEL_LOGIC_ERROR;
        }
    }

    return status;
}

static SIXELSTATUS
sixel_encode_dag_node_load(sixel_encode_dag_context_t *context)
{
    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encode_dag_node_palette_launch(sixel_encode_dag_context_t *context)
{
    SIXELSTATUS status;
    int clustering_pixelformat;

    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (context->palette_ready == 0) {
        return SIXEL_OK;
    }

    status = sixel_encoder_palette_job_init(&context->palette_job,
                                            context->encoder->allocator);
    if (SIXEL_SUCCEEDED(status)) {
        context->palette_job_initialized = 1;
        clustering_pixelformat =
            sixel_encoder_pixelformat_for_colorspace(
                context->encoder->clustering_colorspace,
                context->encoder->prefer_float32);
        status = sixel_encoder_palette_job_launch(&context->palette_job,
                                                  context->frame,
                                                  clustering_pixelformat,
                                                  context->encoder);
        if (SIXEL_SUCCEEDED(status)) {
            context->palette_job_started = 1;
        } else {
            sixel_encoder_palette_job_dispose(&context->palette_job);
            context->palette_job_initialized = 0;
        }
    }

    return status;
}

static SIXELSTATUS
sixel_encode_dag_node_preplan(sixel_encode_dag_context_t *context)
{
    SIXELSTATUS status;
    int height;
    int index;
    sixel_planner_node_kind_t kind;

    status = SIXEL_OK;
    height = 0;
    index = 0;
    kind = SIXEL_PLANNER_NODE_LOAD;

    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    context->clip_config.clip_x = context->encoder->clipx;
    context->clip_config.clip_y = context->encoder->clipy;
    context->clip_config.clip_width = context->encoder->clipwidth;
    context->clip_config.clip_height = context->encoder->clipheight;

    context->resize_config.pixel_width = context->encoder->pixelwidth;
    context->resize_config.pixel_height = context->encoder->pixelheight;
    context->resize_config.percent_width = context->encoder->percentwidth;
    context->resize_config.percent_height = context->encoder->percentheight;
    context->resize_config.method_for_resampling =
        context->encoder->method_for_resampling;
    context->resize_config.prefer_float32 = context->encoder->prefer_float32;
    if (context->planner != NULL) {
        context->resize_config.planner_scale_pixelformat =
            context->planner->scale_pixelformat;
    } else {
        context->resize_config.planner_scale_pixelformat =
            sixel_encoder_pixelformat_for_colorspace(
                context->encoder->working_colorspace,
                context->encoder->prefer_float32);
    }

    context->colors_config.target_pixelformat = context->target_pixelformat;

    height = sixel_frame_get_height(context->frame);
    if (height < 0) {
        height = 0;
    }

    if (context->planner != NULL) {
        /*
         * Phase 3-D: drive the pre-processing chain from DAG node order
         * instead of duplicating clip/colorspace/resize branch logic.
         */
        for (index = 0; index < context->planner->dag_node_count; ++index) {
            kind = context->planner->dag_nodes[index].kind;
            switch (kind) {
            case SIXEL_PLANNER_NODE_CLIP:
                status = sixel_encoder_filter_plan_append(
                    &context->pre_plan,
                    SIXEL_FILTER_KIND_CLIP,
                    &context->clip_config,
                    &context->frame,
                    context->current_pixelformat,
                    context->current_colorspace,
                    context->current_pixelformat,
                    context->current_colorspace,
                    height);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                break;
            case SIXEL_PLANNER_NODE_COLORSPACE_PRE:
                context->colors_config.target_pixelformat =
                    context->planner->scale_input_pixelformat;
                status = sixel_encoder_filter_plan_append(
                    &context->pre_plan,
                    SIXEL_FILTER_KIND_COLORS,
                    &context->colors_config,
                    &context->frame,
                    context->current_pixelformat,
                    context->current_colorspace,
                    context->planner->scale_input_pixelformat,
                    SIXEL_COLORSPACE_LINEAR,
                    height);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                context->current_pixelformat =
                    context->planner->scale_input_pixelformat;
                context->current_colorspace = SIXEL_COLORSPACE_LINEAR;
                break;
            case SIXEL_PLANNER_NODE_SCALE:
                status = sixel_encoder_filter_plan_append(
                    &context->pre_plan,
                    SIXEL_FILTER_KIND_RESIZE,
                    &context->resize_config,
                    &context->frame,
                    context->current_pixelformat,
                    context->current_colorspace,
                    context->planner->scale_pixelformat,
                    context->current_colorspace,
                    height);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                context->current_pixelformat =
                    context->planner->scale_pixelformat;
                break;
            case SIXEL_PLANNER_NODE_COLORSPACE_POST:
                if (context->frame != NULL &&
                    sixel_encoder_frame_preserves_alpha_key(context->frame) &&
                    sixel_encoder_pixelformat_has_alpha(
                        context->current_pixelformat)) {
                    /*
                     * Keep alpha-bearing pixels untouched for the opt-in
                     * tRNS keycolor path. Converting to planner working
                     * formats here would drop alpha before palette build.
                     */
                    break;
                }
                context->colors_config.target_pixelformat =
                    context->planner->working_pixelformat;
                status = sixel_encoder_filter_plan_append(
                    &context->pre_plan,
                    SIXEL_FILTER_KIND_COLORS,
                    &context->colors_config,
                    &context->frame,
                    context->current_pixelformat,
                    context->current_colorspace,
                    context->planner->working_pixelformat,
                    context->planner->working_colorspace_effective,
                    height);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                context->current_pixelformat =
                    context->planner->working_pixelformat;
                context->current_colorspace =
                    context->planner->working_colorspace_effective;
                break;
            default:
                break;
            }
        }
    } else {
        /* Keep legacy serial pre-plan when planner is unavailable. */
        if (context->encoder->clipfirst != 0 && context->clip_active != 0) {
            status = sixel_encoder_filter_plan_append(
                &context->pre_plan,
                SIXEL_FILTER_KIND_CLIP,
                &context->clip_config,
                &context->frame,
                context->current_pixelformat,
                context->current_colorspace,
                context->current_pixelformat,
                context->current_colorspace,
                height);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }

        if (context->encoder->clipfirst == 0 && context->clip_active != 0) {
            status = sixel_encoder_filter_plan_append(
                &context->pre_plan,
                SIXEL_FILTER_KIND_CLIP,
                &context->clip_config,
                &context->frame,
                context->current_pixelformat,
                context->current_colorspace,
                context->current_pixelformat,
                context->current_colorspace,
                height);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }
    }

    status = sixel_encoder_filter_plan_run(&context->pre_plan,
                                           context->encoder->allocator,
                                           context->encoder->logger);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    context->current_pixelformat =
        sixel_frame_get_pixelformat(context->frame);
    context->current_colorspace = sixel_frame_get_colorspace(context->frame);

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encode_dag_node_palette_collect(sixel_encode_dag_context_t *context)
{
    SIXELSTATUS status;
    int quantize_animation_enabled;
    int histogram_colors;
    int method_for_diffuse;
    int skip_palette_diffusion;

    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    quantize_animation_enabled = 0;
    histogram_colors = 0;
    method_for_diffuse = SIXEL_DIFFUSE_NONE;
    skip_palette_diffusion = 0;

    if (context->palette_job_started != 0) {
        status = sixel_encoder_palette_job_wait(&context->palette_job,
                                                &context->async_dither);
        sixel_encoder_palette_job_dispose(&context->palette_job);
        context->palette_job_initialized = 0;
        if (SIXEL_SUCCEEDED(status) && context->async_dither != NULL) {
            context->dither = context->async_dither;
        } else {
            context->palette_job_started = 0;
            context->async_dither = NULL;
        }
    }

    if (context->palette_job_started == 0) {
        status = sixel_encoder_apply_palette_filter(context->encoder,
                                                    &context->frame,
                                                    1,
                                                    &context->dither);
        if (status != SIXEL_OK) {
            context->dither = NULL;
            return status;
        }
        if (context->palette_job_initialized != 0) {
            sixel_encoder_palette_job_dispose(&context->palette_job);
            context->palette_job_initialized = 0;
        }
    }

    if (context->encoder->color_option == SIXEL_COLOR_OPTION_DEFAULT) {
        status = sixel_encoder_apply_quantize_animation_mode(
            context->encoder,
            context->frame,
            context->dither);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        quantize_animation_enabled =
            sixel_encoder_quantize_animation_enabled_for_frame(
                context->encoder,
                context->frame);
        if (quantize_animation_enabled != 0) {
            /*
             * Keep palette slot count fixed while animation quantize mode is
             * active. This avoids frame-to-frame palette header churn in
             * parallel dithering paths.
             */
            context->dither->force_palette = 1;
        }
    }

    status = sixel_encoder_apply_lut_filter(context->encoder,
                                            context->dither);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (context->encoder->dither_cache != NULL) {
        /*
         * LUT preparation may update the cached dither object in place.  Drop
         * the old cache reference before storing the post-LUT reference so a
         * fixed palette cache hit does not accumulate one reference per frame.
         */
        sixel_encoder_clear_dither_cache(context->encoder);
        context->encoder->dither_cache = context->dither;
        sixel_dither_ref(context->dither);
    }

    if (context->encoder->fdrcs) {
        status = sixel_encoder_ensure_cell_size(context->encoder);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (context->encoder->fuse_macro
            || context->encoder->macro_number >= 0) {
            sixel_helper_set_additional_message(
                "drcs option cannot be used together with macro output.");
            return SIXEL_BAD_ARGUMENT;
        }
    }

    if (context->encoder->verbose) {
        if ((sixel_frame_get_pixelformat(context->frame)
            & SIXEL_FORMATTYPE_PALETTE)) {
            sixel_debug_print_palette(context->dither);
        }
    }

    histogram_colors =
        sixel_dither_get_num_of_histogram_colors(context->dither);
    method_for_diffuse = context->encoder->method_for_diffuse;
    if (context->encoder->color_option == SIXEL_COLOR_OPTION_HIGHCOLOR) {
        /*
         * High-color output does not quantize pixels through a palette.
         * Treat palette-space diffusion requests as no-ops here so unsupported
         * interframe policies do not reach the high-color encoder.
         */
        skip_palette_diffusion = 1;
    } else if (context->encoder->color_option == SIXEL_COLOR_OPTION_DEFAULT &&
               histogram_colors >= 0 &&
               histogram_colors <= context->encoder->reqcolors) {
        /*
         * Keep the exact-palette fast path local to this frame.  Unknown
         * histogram counts (-1) occur for externally supplied palettes.  A
         * mapfile image can also report its own palette histogram here, which
         * is not the source image's color count, so fixed-palette paths still
         * need the requested diffusion policy.
         * Updating encoder->method_for_diffuse here races with planner reads
         * when palette jobs run on worker threads.
         */
        skip_palette_diffusion = 1;
    }
    if (skip_palette_diffusion != 0) {
        method_for_diffuse = SIXEL_DIFFUSE_NONE;
    }
    sixel_dither_set_diffusion_type(context->dither, method_for_diffuse);
    context->dither->interframe_strategy_override =
        context->encoder->interframe_strategy_override;
    context->dither->interframe_strategy_token =
        context->encoder->interframe_strategy_token;
    context->dither->interframe_spatial_diffuse_override =
        context->encoder->interframe_spatial_diffuse_override;
    context->dither->interframe_spatial_diffuse =
        context->encoder->interframe_spatial_diffuse;
    context->dither->interframe_noise_strength_override =
        context->encoder->interframe_noise_strength_override;
    context->dither->interframe_noise_strength_u8 =
        context->encoder->interframe_noise_strength_u8;
    context->dither->stbn_motion_adapt_override =
        context->encoder->stbn_motion_adapt_override;
    context->dither->stbn_motion_adapt_enabled =
        context->encoder->stbn_motion_adapt_enabled;
    context->dither->stbn_scene_cut_reset_override =
        context->encoder->stbn_scene_cut_reset_override;
    context->dither->stbn_scene_cut_reset_enabled =
        context->encoder->stbn_scene_cut_reset_enabled;
    context->dither->stbn_scene_detect_override =
        context->encoder->stbn_scene_detect_override;
    context->dither->stbn_scene_detect_enabled =
        context->encoder->stbn_scene_detect_enabled;
    context->dither->stbn_alpha_guard_override =
        context->encoder->stbn_alpha_guard_override;
    context->dither->stbn_alpha_guard_enabled =
        context->encoder->stbn_alpha_guard_enabled;
    context->dither->stbn_perceptual_weight_override =
        context->encoder->stbn_perceptual_weight_override;
    context->dither->stbn_perceptual_weight_enabled =
        context->encoder->stbn_perceptual_weight_enabled;
    context->dither->stbn_fastpath_override =
        context->encoder->stbn_fastpath_override;
    context->dither->stbn_fastpath_enabled =
        context->encoder->stbn_fastpath_enabled;
    context->dither->bluenoise_strength_override =
        context->encoder->bluenoise_strength_override;
    context->dither->bluenoise_strength = context->encoder->bluenoise_strength;
    context->dither->bluenoise_gradient_factor_override =
        context->encoder->bluenoise_gradient_factor_override;
    context->dither->bluenoise_gradient_factor =
        context->encoder->bluenoise_gradient_factor;
    context->dither->bluenoise_phase_override =
        context->encoder->bluenoise_phase_override;
    context->dither->bluenoise_phase_x = context->encoder->bluenoise_phase_x;
    context->dither->bluenoise_phase_y = context->encoder->bluenoise_phase_y;
    context->dither->bluenoise_seed_override =
        context->encoder->bluenoise_seed_override;
    context->dither->bluenoise_seed = context->encoder->bluenoise_seed;
    context->dither->bluenoise_channel_override =
        context->encoder->bluenoise_channel_override;
    context->dither->bluenoise_channel_rgb =
        context->encoder->bluenoise_channel_rgb;
    context->dither->bluenoise_size_override =
        context->encoder->bluenoise_size_override;
    context->dither->bluenoise_size = context->encoder->bluenoise_size;
    context->dither->lut_policy_shared_instance_override =
        context->encoder->lut_policy_shared_instance_override;
    context->dither->lut_policy_shared_instance =
        context->encoder->lut_policy_shared_instance;
    sixel_dither_set_diffusion_scan(context->dither,
                                    context->encoder->method_for_scan);

    if (sixel_trace_topic_is_enabled("dither_contract")) {
        sixel_encoder_update_diagnostic_dither(context->encoder,
                                               context->dither);
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encode_dag_node_dither_plan(sixel_encode_dag_context_t *context)
{
    SIXELSTATUS status;
    int height;
    int index;
    int appended_dither;
    sixel_planner_node_kind_t kind;

    status = SIXEL_OK;
    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    index = 0;
    appended_dither = 0;
    kind = SIXEL_PLANNER_NODE_DITHER;
    context->dither_config.dither = context->dither;
    context->gradient_config.dither = context->dither;
    height = sixel_frame_get_height(context->frame);
    if (height < 0) {
        height = 0;
    }

    if (context->planner != NULL) {
        for (index = 0; index < context->planner->dag_node_count; ++index) {
            kind = context->planner->dag_nodes[index].kind;
            switch (kind) {
            case SIXEL_PLANNER_NODE_GRADIENT_MAP:
                status = sixel_encoder_filter_plan_append(
                    &context->post_plan,
                    SIXEL_FILTER_KIND_GRADIENT,
                    &context->gradient_config,
                    &context->frame,
                    context->current_pixelformat,
                    context->current_colorspace,
                    context->current_pixelformat,
                    context->current_colorspace,
                    height);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                break;
            case SIXEL_PLANNER_NODE_DITHER:
                status = sixel_encoder_filter_plan_append(
                    &context->post_plan,
                    SIXEL_FILTER_KIND_DITHER,
                    &context->dither_config,
                    &context->frame,
                    context->current_pixelformat,
                    context->current_colorspace,
                    context->current_pixelformat,
                    context->current_colorspace,
                    height);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                appended_dither = 1;
                break;
            default:
                break;
            }
        }
    }

    if (appended_dither == 0) {
        status = sixel_encoder_filter_plan_append(
            &context->post_plan,
            SIXEL_FILTER_KIND_DITHER,
            &context->dither_config,
            &context->frame,
            context->current_pixelformat,
            context->current_colorspace,
            context->current_pixelformat,
            context->current_colorspace,
            height);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    status = sixel_encoder_filter_plan_run(&context->post_plan,
                                           context->encoder->allocator,
                                           context->encoder->logger);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encode_dag_node_output(sixel_encode_dag_context_t *context)
{
    SIXELSTATUS status;
    int height;
    int nwrite;
    int outfd_is_tty;
    int should_hide_cursor;
    char const *hide_cursor_env;

    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = SIXEL_OK;
    height = 0;
    outfd_is_tty = 0;
    should_hide_cursor = 0;
    hide_cursor_env = NULL;

    if (context->output) {
        sixel_output_ref(context->output);
        context->fn_write = sixel_encoder_core_write_callback;
        context->write_callback = sixel_encoder_core_write_callback;
        context->write_priv = context->output;
    } else {
        if (context->encoder->fuse_macro
            || context->encoder->macro_number >= 0) {
            context->fn_write = sixel_hex_write_callback;
        } else {
            context->fn_write = sixel_write_callback;
        }
        context->write_callback = context->fn_write;
        context->write_priv = &context->encoder->outfd;
        status = sixel_encoder_core_create_output_from_factory(
            &context->output,
            context->write_callback,
            context->write_priv,
            context->encoder->allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        context->write_callback = sixel_encoder_core_write_callback;
        context->write_priv = context->output;
    }

    if (context->encoder->fdrcs) {
        sixel_output_set_skip_dcs_envelope(context->output, 1);
        sixel_output_set_skip_header(context->output, 1);
    }

    sixel_output_set_8bit_availability(context->output,
                                       context->encoder->f8bit);
    sixel_output_set_gri_arg_limit(context->output,
                                   context->encoder->has_gri_arg_limit);
    sixel_output_set_palette_type(context->output,
                                  context->encoder->palette_type);
    sixel_output_set_penetrate_multiplexer(
        context->output, context->encoder->penetrate_multiplexer);
    sixel_output_set_encode_policy(context->output,
                                   context->encoder->encode_policy);
    sixel_output_set_ormode(context->output, context->encoder->ormode);

    /*
     * Check cancellation before issuing DECRC for animated updates.
     * This keeps the cursor at the bottom-left of the last rendered frame
     * when SIGINT arrives between frames.
     */
    if (sixel_encoder_is_cancel_requested(context->encoder) != 0) {
        return SIXEL_INTERRUPTED;
    }

    if (context->multiframe != 0 && !context->encoder->fstatic) {
        outfd_is_tty = sixel_compat_isatty(context->encoder->outfd);
        hide_cursor_env = sixel_compat_getenv(
            SIXEL_ENCODER_ANIMATION_HIDE_CURSOR_ENVVAR);
        should_hide_cursor = sixel_encoder_should_hide_animation_cursor(
            context->multiframe,
            context->encoder->fstatic,
            outfd_is_tty,
            hide_cursor_env);
        if (should_hide_cursor != 0) {
            (void)sixel_tty_begin_animation_input_guard();
            (void)sixel_tty_hide_cursor(context->encoder->outfd);
        }
        if (context->loop_no != 0 || context->frame_no != 0) {
            context->is_animation = 1;
        }
        height = sixel_frame_get_height(context->frame);
        context->scroll_callback = sixel_write_callback;
        context->scroll_priv = &context->encoder->outfd;
        (void)sixel_tty_scroll(context->scroll_callback,
                               context->scroll_priv,
                               context->encoder->outfd,
                               height,
                               context->is_animation);
    }

    if (context->encoder->fdrcs) {
        status = sixel_drcs_emit_begin_sequence(context->encoder,
                                                context->write_callback,
                                                context->write_priv);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    if (context->encoder->fuse_macro) {
        status = sixel_encoder_output_with_macro(context->frame,
                                                 context->dither,
                                                 context->output,
                                                 context->encoder,
                                                 context->frame_no,
                                                 context->loop_no,
                                                 context->delay);
    } else if (context->encoder->macro_number >= 0) {
        status = sixel_encoder_output_with_macro(context->frame,
                                                 context->dither,
                                                 context->output,
                                                 context->encoder,
                                                 context->frame_no,
                                                 context->loop_no,
                                                 context->delay);
    } else {
        status = sixel_encoder_output_without_macro(context->frame,
                                                    context->dither,
                                                    context->output,
                                                    context->encoder,
                                                    context->frame_no,
                                                    context->loop_no,
                                                    context->delay);
    }
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (sixel_encoder_is_cancel_requested(context->encoder) != 0) {
        nwrite = context->write_callback("\x18\033\\", 3,
                                         context->write_priv);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: write_callback() failed.");
            return status;
        }
        if (context->multiframe != 0 &&
            !context->encoder->fstatic
            && outfd_is_tty != 0
            && height > 0) {
            (void)sixel_tty_restore_animation_cursor_to_bottom(
                context->encoder->outfd,
                height);
        }
        return SIXEL_INTERRUPTED;
    }

    if (context->encoder->fdrcs) {
        if (context->encoder->f8bit) {
            nwrite = context->write_callback("\234", 1,
                                             context->write_priv);
        } else {
            nwrite = context->write_callback("\033\\", 2,
                                             context->write_priv);
        }
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: write_callback() failed.");
            return status;
        }

        if (context->encoder->tile_outfd >= 0) {
            status = sixel_drcs_emit_tile_chars(context->encoder,
                                                context->frame);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }
    }

    return SIXEL_OK;
}


static SIXELSTATUS
sixel_encoder_copy_samples(sixel_encoder_t *encoder,
                           sixel_frame_t *frame,
                           sixel_allocator_t *allocator,
                           sixel_frame_t **sample_out)
{
    SIXELSTATUS status;
    sixel_filter_sample_config_t config;

    if (encoder == NULL || frame == NULL || sample_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(&config, 0, sizeof(config));
    config.clip_x = encoder->clipx;
    config.clip_y = encoder->clipy;
    config.clip_width = encoder->clipwidth;
    config.clip_height = encoder->clipheight;
    config.reqcolors = encoder->reqcolors;
    config.quality_mode = encoder->quality_mode;
    config.palette_sample_override = encoder->palette_sample_override;
    config.palette_sample_target = encoder->palette_sample_target;

    status = sixel_filter_sample_frame(&config,
                                       frame,
                                       allocator,
                                       sample_out,
                                       encoder->logger);

    return status;
}


static int
sixel_encoder_palette_job_thread(void *priv)
{
    sixel_palette_async_job_t *job;
    SIXELSTATUS status;
    sixel_dither_t *local;
    int preserve_alpha_key;
    sixel_timeline_logger_t *logger;

    job = (sixel_palette_async_job_t *)priv;
    if (job == NULL) {
        return 0;
    }
    status = SIXEL_BAD_ARGUMENT;
    local = NULL;
    preserve_alpha_key = 0;
    logger = NULL;

    if (job != NULL) {
        logger = job->logger;
    }
    if (logger != NULL) {
        sixel_timeline_logger_set_frame_context(logger,
                                       job->frame_no,
                                       job->loop_no,
                                       job->multiframe);
    }

    if (job != NULL && job->encoder != NULL && job->sample_frame != NULL) {
        preserve_alpha_key = sixel_encoder_frame_preserves_alpha_key(
            job->sample_frame);
        if (!preserve_alpha_key) {
            status = sixel_frame_set_pixelformat(job->sample_frame,
                                                 job->target_pixelformat);
        } else {
            status = SIXEL_OK;
        }
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_encoder_apply_palette_filter(job->encoder,
                                                        &job->sample_frame,
                                                        0,
                                                        &local);
        }
    }

    sixel_mutex_lock(&job->mutex);
    job->status = status;
    job->dither = local;
    job->finished = 1;
    sixel_cond_broadcast(&job->cond);
    sixel_mutex_unlock(&job->mutex);

    if (SIXEL_FAILED(status) && local != NULL) {
        sixel_dither_unref(local);
    }
    if (logger != NULL) {
        sixel_timeline_logger_clear_frame_context(logger);
    }

    return 0;
}


static SIXELSTATUS
sixel_encoder_palette_job_init(sixel_palette_async_job_t *job,
                               sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int result;

    if (job == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    job->encoder = NULL;
    job->logger = NULL;
    job->sample_frame = NULL;
    job->allocator = allocator;
    job->dither = NULL;
    job->status = SIXEL_OK;
    job->target_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    job->reqcolors = 0;
    job->method_for_largest = SIXEL_LARGE_AUTO;
    job->method_for_rep = SIXEL_REP_AUTO;
    job->quality_mode = SIXEL_QUALITY_AUTO;
    job->lut_policy = SIXEL_LUT_POLICY_AUTO;
    job->final_merge_mode = SIXEL_FINAL_MERGE_AUTO;
    job->sixel_reversible = 0;
    job->quantize_model = SIXEL_QUANTIZE_MODEL_AUTO;
    job->force_palette = 0;
    job->frame_no = 0;
    job->loop_no = 0;
    job->multiframe = 0;
    job->started = 0;
    job->finished = 0;

    result = sixel_mutex_init(&job->mutex);
    if (result != 0) {
        return SIXEL_RUNTIME_ERROR;
    }
    result = sixel_cond_init(&job->cond);
    if (result != 0) {
        sixel_mutex_destroy(&job->mutex);
        return SIXEL_RUNTIME_ERROR;
    }

    status = SIXEL_OK;

    return status;
}


static void
sixel_encoder_palette_job_dispose(sixel_palette_async_job_t *job)
{
    if (job == NULL) {
        return;
    }
    if (job->sample_frame != NULL) {
        sixel_frame_unref(job->sample_frame);
        job->sample_frame = NULL;
    }
    job->encoder = NULL;
    job->logger = NULL;
    if (job->dither != NULL) {
        sixel_dither_unref(job->dither);
        job->dither = NULL;
    }
    sixel_cond_destroy(&job->cond);
    sixel_mutex_destroy(&job->mutex);
}


static SIXELSTATUS
sixel_encoder_palette_job_launch(sixel_palette_async_job_t *job,
                                 sixel_frame_t *frame,
                                 int target_pixelformat,
                                 sixel_encoder_t *encoder)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int result;

    if (job == NULL || frame == NULL || encoder == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    job->encoder = encoder;
    job->logger = encoder->logger;
    job->target_pixelformat = target_pixelformat;
    job->reqcolors = encoder->reqcolors;
    job->method_for_largest = encoder->method_for_largest;
    job->method_for_rep = encoder->method_for_rep;
    job->quality_mode = encoder->quality_mode;
    job->lut_policy = encoder->lut_policy;
    job->final_merge_mode = encoder->final_merge_mode;
    job->sixel_reversible = encoder->sixel_reversible;
    job->quantize_model = encoder->quantize_model;
    job->force_palette = encoder->force_palette;
    job->frame_no = sixel_frame_get_frame_no(frame);
    job->loop_no = sixel_frame_get_loop_no(frame);
    job->multiframe = sixel_frame_get_multiframe(frame);

    status = sixel_encoder_copy_samples(encoder,
                                        frame,
                                        encoder->allocator,
                                        &job->sample_frame);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    result = sixel_thread_create(&job->thread,
                                 sixel_encoder_palette_job_thread,
                                 job);
    if (result != 0) {
        sixel_frame_unref(job->sample_frame);
        job->sample_frame = NULL;
        return SIXEL_RUNTIME_ERROR;
    }

    job->started = 1;

    return SIXEL_OK;
}


static SIXELSTATUS
sixel_encoder_palette_job_wait(sixel_palette_async_job_t *job,
                               sixel_dither_t **dither_out)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (job == NULL || dither_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *dither_out = NULL;

    if (job->started == 0) {
        return SIXEL_LOGIC_ERROR;
    }

    sixel_mutex_lock(&job->mutex);
    while (!job->finished) {
        sixel_cond_wait(&job->cond, &job->mutex);
    }
    sixel_mutex_unlock(&job->mutex);

    sixel_thread_join(&job->thread);

    status = job->status;
    if (SIXEL_SUCCEEDED(status)) {
        *dither_out = job->dither;
        job->dither = NULL;
    }

    return status;
}


/* create palette from specified map file */
static SIXELSTATUS
sixel_prepare_specified_palette(
    sixel_dither_t  /* out */   **dither,
    sixel_encoder_t /* in */    *encoder)
{
    SIXELSTATUS status;
    sixel_callback_context_for_mapfile_t callback_context;
    sixel_loader_t *loader;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_override;
    char const *path;
    sixel_palette_format_t format_hint;
    sixel_palette_format_t format_ext;
    sixel_palette_format_t format_final;
    sixel_palette_format_t format_detected;
    FILE *stream;
    int close_stream;
    unsigned char *buffer;
    size_t buffer_size;
    int palette_request;
    int need_detection;
    int treat_as_image;
    int path_has_extension;
    char mapfile_message[256];

    status = SIXEL_FALSE;
    loader = NULL;
    fstatic = 1;
    fuse_palette = 1;
    reqcolors = SIXEL_PALETTE_MAX;
    loop_override = SIXEL_LOOP_DISABLE;
    path = NULL;
    format_hint = SIXEL_PALETTE_FORMAT_NONE;
    format_ext = SIXEL_PALETTE_FORMAT_NONE;
    format_final = SIXEL_PALETTE_FORMAT_NONE;
    format_detected = SIXEL_PALETTE_FORMAT_NONE;
    stream = NULL;
    close_stream = 0;
    buffer = NULL;
    buffer_size = 0u;
    palette_request = 0;
    need_detection = 0;
    treat_as_image = 0;
    path_has_extension = 0;
    mapfile_message[0] = '\0';

    if (dither == NULL || encoder == NULL || encoder->mapfile == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prepare_specified_palette: invalid mapfile path.");
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_encoder_log_stage(encoder,
                            NULL,
                            "palette",
                            "worker",
                            "start",
                            "mapfile=%s",
                            encoder->mapfile);

    path = sixel_palette_strip_prefix(encoder->mapfile, &format_hint);
    if (path == NULL || *path == '\0') {
        sixel_helper_set_additional_message(
            "sixel_prepare_specified_palette: empty mapfile path.");
        return SIXEL_BAD_ARGUMENT;
    }

    format_ext = sixel_palette_format_from_extension(path);
    path_has_extension = sixel_path_has_any_extension(path);

    if (format_hint != SIXEL_PALETTE_FORMAT_NONE) {
        palette_request = 1;
        format_final = format_hint;
    } else if (format_ext != SIXEL_PALETTE_FORMAT_NONE) {
        palette_request = 1;
        format_final = format_ext;
    } else if (!path_has_extension) {
        palette_request = 1;
        need_detection = 1;
    } else {
        treat_as_image = 1;
    }

    if (palette_request) {
        status = sixel_palette_open_read(path, &stream, &close_stream);
        if (SIXEL_FAILED(status)) {
            goto palette_cleanup;
        }
        status = sixel_palette_read_stream(stream,
                                           encoder->allocator,
                                           &buffer,
                                           &buffer_size);
        if (close_stream) {
            sixel_palette_close_stream(stream, close_stream);
            stream = NULL;
            close_stream = 0;
        }
        if (SIXEL_FAILED(status)) {
            goto palette_cleanup;
        }
        if (buffer_size == 0u) {
            sixel_compat_snprintf(mapfile_message,
                                  sizeof(mapfile_message),
                                  "sixel_prepare_specified_palette: "
                                  "mapfile \"%s\" is empty.",
                                  path != NULL ? path : "");
            sixel_helper_set_additional_message(mapfile_message);
            status = SIXEL_BAD_INPUT;
            goto palette_cleanup;
        }

        if (format_final == SIXEL_PALETTE_FORMAT_NONE) {
            format_detected = sixel_palette_guess_format(buffer,
                                                         buffer_size);
            if (format_detected == SIXEL_PALETTE_FORMAT_NONE) {
                sixel_helper_set_additional_message(
                    "sixel_prepare_specified_palette: "
                    "unable to detect palette format.");
                status = SIXEL_BAD_INPUT;
                goto palette_cleanup;
            }
            if (need_detection
                    && format_detected == SIXEL_PALETTE_FORMAT_ACT) {
                sixel_helper_set_additional_message(
                    "sixel_prepare_specified_palette: ambiguous ACT "
                    "payload without extension; use act:PATH.");
                status = SIXEL_BAD_INPUT;
                goto palette_cleanup;
            }
            format_final = format_detected;
        } else if (format_final == SIXEL_PALETTE_FORMAT_PAL_AUTO) {
            format_detected = sixel_palette_guess_format(buffer,
                                                         buffer_size);
            if (format_detected == SIXEL_PALETTE_FORMAT_PAL_JASC ||
                    format_detected == SIXEL_PALETTE_FORMAT_PAL_RIFF) {
                format_final = format_detected;
            } else {
                sixel_helper_set_additional_message(
                    "sixel_prepare_specified_palette: "
                    "ambiguous .pal content.");
                status = SIXEL_BAD_INPUT;
                goto palette_cleanup;
            }
        } else if (need_detection) {
            format_detected = sixel_palette_guess_format(buffer,
                                                         buffer_size);
            if (format_detected == SIXEL_PALETTE_FORMAT_NONE) {
                sixel_helper_set_additional_message(
                    "sixel_prepare_specified_palette: "
                    "unable to detect palette format.");
                status = SIXEL_BAD_INPUT;
                goto palette_cleanup;
            }
            format_final = format_detected;
        }

        switch (format_final) {
        case SIXEL_PALETTE_FORMAT_ACT:
            status = sixel_palette_parse_act(buffer,
                                             buffer_size,
                                             encoder,
                                             dither);
            break;
        case SIXEL_PALETTE_FORMAT_PAL_JASC:
            status = sixel_palette_parse_pal_jasc(buffer,
                                                  buffer_size,
                                                  encoder,
                                                  dither);
            break;
        case SIXEL_PALETTE_FORMAT_PAL_RIFF:
            status = sixel_palette_parse_pal_riff(buffer,
                                                  buffer_size,
                                                  encoder,
                                                  dither);
            break;
        case SIXEL_PALETTE_FORMAT_GPL:
            status = sixel_palette_parse_gpl(buffer,
                                             buffer_size,
                                             encoder,
                                             dither);
            break;
        default:
            sixel_helper_set_additional_message(
                "sixel_prepare_specified_palette: "
                "unsupported palette format.");
            status = SIXEL_BAD_INPUT;
            break;
        }

palette_cleanup:
        if (buffer != NULL) {
            sixel_allocator_free(encoder->allocator, buffer);
            buffer = NULL;
        }
        if (stream != NULL) {
            sixel_palette_close_stream(stream, close_stream);
            stream = NULL;
        }
        if (SIXEL_SUCCEEDED(status)) {
            return status;
        }
        if (!treat_as_image) {
            return status;
        }
    }

    callback_context.reqcolors = encoder->reqcolors;
    callback_context.dither = NULL;
    callback_context.allocator = encoder->allocator;
    callback_context.working_colorspace = encoder->working_colorspace;
    callback_context.lut_policy = encoder->lut_policy;
    callback_context.lut_policy_shared_instance_override =
        encoder->lut_policy_shared_instance_override;
    callback_context.lut_policy_shared_instance =
        encoder->lut_policy_shared_instance;
    callback_context.prefer_float32 = encoder->prefer_float32;

    sixel_helper_set_thumbnail_size_hint(
        sixel_encoder_thumbnail_hint(encoder));
    status = sixel_loader_new(&loader, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                 &fstatic);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_USE_PALETTE,
                                 &fuse_palette);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQCOLORS,
                                 &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_BGCOLOR,
                                 encoder->bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }
    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_BGCOLOR_SOURCE,
                                 &encoder->bgcolor_source);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                 &loop_override);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_INSECURE,
                                 &encoder->finsecure);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CANCEL_FLAG,
                                 encoder->cancel_flag);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }
    status = sixel_loader_set_cancel_callback(loader,
                                              encoder->cancel_function,
                                              encoder->cancel_context);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 encoder->loader_order);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_START_FRAME_NO,
                                 encoder->loader_start_frame_no_set
                                     ? &encoder->loader_start_frame_no
                                     : NULL);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 &callback_context);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_load_file(loader,
                                    encoder->mapfile,
                                    load_image_callback_for_palette);
    if (status != SIXEL_OK) {
        goto end_loader;
    }

end_loader:
    sixel_loader_unref(loader);

    if (status != SIXEL_OK) {
        return status;
    }

    if (! callback_context.dither) {
        sixel_compat_snprintf(mapfile_message,
                              sizeof(mapfile_message),
                              "sixel_prepare_specified_palette() failed.\n"
                              "reason: mapfile \"%s\" is empty.",
                              encoder->mapfile != NULL
                                ? encoder->mapfile
                                : "");
        sixel_helper_set_additional_message(mapfile_message);
        return SIXEL_BAD_INPUT;
    }

    *dither = callback_context.dither;

    sixel_encoder_log_stage(encoder,
                            NULL,
                            "palette",
                            "worker",
                            "finish",
                            "mapfile=%s format=%d",
                            encoder->mapfile,
                            format_final);

    return status;
}

static int
sixel_encoder_pixelformat_has_alpha(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
        return 1;
    default:
        return 0;
    }
}

static SIXELSTATUS
sixel_encoder_attach_alpha_keycolor(sixel_encoder_t *encoder,
                                    sixel_dither_t *dither)
{
    SIXELSTATUS status;
    unsigned char *expanded;
    unsigned int base_colors;
    unsigned int key_index;
    size_t bytes;
    sixel_palette_entries_request_t entries_request;
    sixel_palette_entries_view_t entries_view;
    sixel_palette_float32_entries_request_t float32_request;

    status = SIXEL_FALSE;
    expanded = NULL;
    base_colors = 0U;
    key_index = 0U;
    bytes = 0U;
    memset(&entries_request, 0, sizeof(entries_request));
    memset(&entries_view, 0, sizeof(entries_view));
    memset(&float32_request, 0, sizeof(float32_request));

    if (encoder == NULL || dither == NULL || dither->palette == NULL ||
        dither->palette->vtbl == NULL ||
        dither->palette->vtbl->get_entries == NULL ||
        dither->palette->vtbl->init_entries == NULL ||
        dither->palette->vtbl->init_entries_float32 == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = dither->palette->vtbl->get_entries(dither->palette,
                                                &entries_view);
    if (SIXEL_FAILED(status) || entries_view.entries == NULL ||
            entries_view.depth < 3) {
        return SIXEL_BAD_ARGUMENT;
    }

    base_colors = (unsigned int)dither->ncolors;
    if (base_colors == 0U || base_colors >= (unsigned int)SIXEL_PALETTE_MAX) {
        return SIXEL_BAD_INPUT;
    }

    key_index = base_colors;
    bytes = (size_t)(base_colors + 1U) * 3U;
    expanded = (unsigned char *)sixel_allocator_malloc(dither->allocator, bytes);
    if (expanded == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_attach_alpha_keycolor: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memcpy(expanded, entries_view.entries, (size_t)base_colors * 3U);
    if (encoder->bgcolor != NULL) {
        expanded[key_index * 3U + 0U] = encoder->bgcolor[0];
        expanded[key_index * 3U + 1U] = encoder->bgcolor[1];
        expanded[key_index * 3U + 2U] = encoder->bgcolor[2];
    } else {
        expanded[key_index * 3U + 0U] = 0U;
        expanded[key_index * 3U + 1U] = 0U;
        expanded[key_index * 3U + 2U] = 0U;
    }

    entries_request.entries = expanded;
    entries_request.colors = base_colors + 1U;
    entries_request.depth = 3;
    status = dither->palette->vtbl->init_entries(dither->palette,
                                                 &entries_request);
    sixel_allocator_free(dither->allocator, expanded);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    float32_request.entries = NULL;
    float32_request.colors = 0U;
    float32_request.depth = 0;
    (void)dither->palette->vtbl->init_entries_float32(dither->palette,
                                                      &float32_request);
    dither->ncolors = (int)(base_colors + 1U);
    dither->keycolor = (int)key_index;

    return SIXEL_OK;
}


/* create dither object from a frame */
static SIXELSTATUS
sixel_encoder_prepare_palette(
    sixel_encoder_t *encoder,  /* encoder object */
    sixel_frame_t   *frame,    /* input frame object */
    sixel_dither_t  **dither,  /* dither object to be created from the frame */
    int allow_cache,
    sixel_timeline_logger_t *logger)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_filter_final_merge_config_t merge_config;
    sixel_timeline_logger_t *target_logger;
    int cache_allowed;
    sixel_frame_t *palette_frame;
    sixel_frame_t *cluster_frame;
    unsigned char *palette_pixels;
    int palette_pixelformat;
    int palette_target_pixelformat;
    int clustering_colorspace;
    int working_colorspace;
    int prefer_float32;
    int reserve_alpha_key;
    int palette_reqcolors;
    int quantize_override_lock_acquired;
    int quantize_animation_enabled;
    int effective_method_for_largest;
    int effective_final_merge_mode;
    int effective_merge_oversplit_override;
    double effective_merge_oversplit;
    int effective_merge_lloyd_override;
    unsigned int effective_merge_lloyd;
    int effective_lut_policy;
    int effective_lut_policy_override;
    int fixed_palette_cache_candidate;
    int dither_cache_hit;

    target_logger = logger;
    cache_allowed = allow_cache != 0;
    palette_frame = frame;
    cluster_frame = NULL;
    palette_pixels = NULL;
    palette_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    palette_target_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    clustering_colorspace = SIXEL_COLORSPACE_GAMMA;
    working_colorspace = SIXEL_COLORSPACE_GAMMA;
    prefer_float32 = 0;
    reserve_alpha_key = 0;
    palette_reqcolors = 0;
    quantize_override_lock_acquired = 0;
    quantize_animation_enabled = 0;
    effective_method_for_largest = SIXEL_LARGE_AUTO;
    effective_final_merge_mode = SIXEL_FINAL_MERGE_AUTO;
    effective_merge_oversplit_override = 0;
    effective_merge_oversplit = 1.81;
    effective_merge_lloyd_override = 0;
    effective_merge_lloyd = 3u;
    effective_lut_policy = SIXEL_LUT_POLICY_CERTLUT;
    effective_lut_policy_override = 0;
    fixed_palette_cache_candidate = 0;
    dither_cache_hit = 0;
    if (encoder == NULL || frame == NULL || dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (encoder->color_option != SIXEL_COLOR_OPTION_DEFAULT) {
        sixel_encoder_reset_quantize_animation_state(encoder);
    }
    if (encoder != NULL) {
        if (target_logger == NULL) {
            target_logger = encoder->logger;
        }
    }
    quantize_animation_enabled =
        sixel_encoder_quantize_animation_enabled_for_frame(encoder, frame);

    if ((sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_PALETTE) != 0
            && quantize_animation_enabled != 0) {
        /*
         * Animation quantize mode needs a stable quantizer path across frames.
         * Promote indexed input to RGB888 so palette lock logic can run.
         */
        status = sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_RGB888);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    switch (encoder->color_option) {
    case SIXEL_COLOR_OPTION_HIGHCOLOR:
        if (cache_allowed && encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_dither_new(dither, (-1), encoder->allocator);
            sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
        }
        goto end;
    case SIXEL_COLOR_OPTION_MONOCHROME:
        fixed_palette_cache_candidate = 1;
        if (cache_allowed && encoder->dither_cache) {
            *dither = encoder->dither_cache;
            sixel_dither_ref(*dither);
            dither_cache_hit = 1;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_monochrome_palette(dither, encoder->finvert);
        }
        goto end;
    case SIXEL_COLOR_OPTION_MAPFILE:
        fixed_palette_cache_candidate = 1;
        if (cache_allowed && encoder->dither_cache) {
            *dither = encoder->dither_cache;
            sixel_dither_ref(*dither);
            dither_cache_hit = 1;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_specified_palette(dither, encoder);
        }
        goto end;
    case SIXEL_COLOR_OPTION_BUILTIN:
        fixed_palette_cache_candidate = 1;
        if (cache_allowed && encoder->dither_cache) {
            *dither = encoder->dither_cache;
            sixel_dither_ref(*dither);
            dither_cache_hit = 1;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_builtin_palette(dither, encoder->builtin_palette);
        }
        goto end;
    case SIXEL_COLOR_OPTION_DEFAULT:
    default:
        break;
    }

    if (sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_PALETTE) {
        if (!sixel_frame_get_palette(frame)) {
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        status = sixel_dither_new(dither, sixel_frame_get_ncolors(frame),
                                  encoder->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_dither_set_palette(*dither, sixel_frame_get_palette(frame));
        sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
        if (sixel_frame_get_transparent(frame) != (-1)) {
            sixel_dither_set_transparent(*dither, sixel_frame_get_transparent(frame));
        }
        if (*dither && cache_allowed && encoder->dither_cache) {
            sixel_dither_unref(encoder->dither_cache);
        }
        goto end;
    }

    if (sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_GRAYSCALE) {
        switch (sixel_frame_get_pixelformat(frame)) {
        case SIXEL_PIXELFORMAT_G1:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G1);
            break;
        case SIXEL_PIXELFORMAT_G2:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G2);
            break;
        case SIXEL_PIXELFORMAT_G4:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G4);
            break;
        case SIXEL_PIXELFORMAT_G8:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G8);
            break;
        default:
            *dither = NULL;
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        if (*dither && cache_allowed && encoder->dither_cache) {
            sixel_dither_unref(encoder->dither_cache);
        }
        sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
        status = SIXEL_OK;
        goto end;
    }

    if (cache_allowed && encoder->dither_cache) {
        sixel_dither_unref(encoder->dither_cache);
    }
    reserve_alpha_key =
        encoder->reqcolors > 1
        && sixel_encoder_frame_preserves_alpha_key(frame);
    palette_reqcolors = encoder->reqcolors;
    if (reserve_alpha_key) {
        palette_reqcolors = encoder->reqcolors - 1;
    }
    effective_method_for_largest = SIXEL_LARGE_NORM;
    if (encoder->quantize_model == SIXEL_QUANTIZE_MODEL_MEDIANCUT) {
        /*
         * Largest-axis selection only applies to the Heckbert median-cut
         * palette builder.
         */
        effective_method_for_largest = encoder->method_for_largest;
    }
    effective_final_merge_mode = encoder->final_merge_mode;
    effective_merge_oversplit_override
        = encoder->quantize_model_merge_oversplit_override;
    effective_merge_oversplit = encoder->quantize_model_merge_oversplit;
    effective_merge_lloyd_override
        = encoder->quantize_model_merge_lloyd_override;
    effective_merge_lloyd = encoder->quantize_model_merge_lloyd;
    effective_lut_policy = encoder->lut_policy;
    effective_lut_policy_override = encoder->lut_policy_override;
    if (encoder->quantize_model == SIXEL_QUANTIZE_MODEL_MEDIANCUT
            && encoder->quantize_model_heckbert_profile
                == SIXEL_HECKBERT_PROFILE_QUALITY
            && effective_merge_oversplit_override == 0
            && effective_final_merge_mode == SIXEL_FINAL_MERGE_WARD) {
        effective_merge_oversplit =
            sixel_encoder_heckbert_quality_oversplit_factor(palette_reqcolors);
        effective_merge_oversplit_override = 1;
        if (effective_merge_lloyd_override == 0) {
            /*
             * Keep profile=quality defaults effective without changing CLI
             * precedence: explicit merge_lloyd still wins.
             */
            effective_merge_lloyd = 2u;
            effective_merge_lloyd_override = 1;
        }
    }
    if (encoder->quantize_model == SIXEL_QUANTIZE_MODEL_MEDIANCUT
            && encoder->quantize_model_heckbert_profile
                == SIXEL_HECKBERT_PROFILE_SPEED
            && effective_lut_policy_override == 0
            && encoder->quantize_model_merge_override == 0
            && encoder->method_for_largest_override == 0
            && effective_lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        /*
         * Keep speed profile internal and compatible: when lookup policy is not
         * explicitly pinned by user/env, use the lighter 5bit histogram bins.
         */
        effective_lut_policy = SIXEL_LUT_POLICY_5BIT;
    }

    status = sixel_dither_new(dither, palette_reqcolors, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (reserve_alpha_key) {
        /*
         * Signal dither_initialize() to keep alpha-bearing input so palette
         * generation can ignore fully transparent pixels.
         */
        sixel_dither_set_transparent(*dither, 0);
        sixel_dither_set_transparent_bgcolor_hint(*dither,
                                                  encoder->bgcolor);
    } else {
        sixel_dither_clear_transparent_bgcolor_hint(*dither);
    }

    clustering_colorspace = encoder->clustering_colorspace;
    working_colorspace = encoder->working_colorspace;
    prefer_float32 = encoder->prefer_float32;
    if (reserve_alpha_key) {
        clustering_colorspace = sixel_frame_get_colorspace(frame);
        palette_target_pixelformat = sixel_frame_get_pixelformat(frame);
    } else {
        palette_target_pixelformat =
            sixel_encoder_pixelformat_for_colorspace(clustering_colorspace,
                                                     prefer_float32);
    }

    if (!reserve_alpha_key &&
        (sixel_frame_get_pixelformat(frame) != palette_target_pixelformat
            || sixel_frame_get_colorspace(frame)
                != clustering_colorspace)) {
        status = sixel_encoder_frame_clone(frame,
                                           encoder->allocator,
                                           &cluster_frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        palette_frame = cluster_frame;
        if (sixel_frame_get_pixelformat(palette_frame)
                != palette_target_pixelformat) {
            status = sixel_frame_set_pixelformat(palette_frame,
                                                 palette_target_pixelformat);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
        if (sixel_frame_get_colorspace(palette_frame)
                != clustering_colorspace) {
            status = sixel_encoder_convert_frame_colorspace(
                palette_frame,
                clustering_colorspace);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
    }

    sixel_dither_set_lut_policy(*dither, effective_lut_policy);
    (*dither)->lut_policy_shared_instance_override =
        encoder->lut_policy_shared_instance_override;
    (*dither)->lut_policy_shared_instance =
        encoder->lut_policy_shared_instance;
    sixel_dither_set_sixel_reversible(*dither,
                                      encoder->sixel_reversible);
    memset(&merge_config, 0, sizeof(merge_config));
    merge_config.dither = *dither;
    merge_config.final_merge_mode = effective_final_merge_mode;
    status = sixel_filter_final_merge_apply(&merge_config, target_logger);
    if (SIXEL_FAILED(status)) {
        sixel_dither_unref(*dither);
        goto end;
    }
    (*dither)->quantize_model = encoder->quantize_model;

    palette_pixels = sixel_frame_get_pixels(palette_frame);
    palette_pixelformat = sixel_frame_get_pixelformat(palette_frame);
    quantize_override_lock_acquired =
        sixel_encoder_quantize_override_lock_acquire();
    sixel_set_kmeans_init_type_override(
        encoder->quantize_model_kmeans_init_override,
        (sixel_kmeans_init_type)encoder->quantize_model_kmeans_init_type);
    sixel_set_kmeans_threshold_override(
        encoder->quantize_model_kmeans_threshold_override,
        encoder->quantize_model_kmeans_threshold);
    sixel_set_kmeans_binning_mode_override(
        encoder->quantize_model_kmeans_binning_override,
        (sixel_kmeans_binning_mode)
            encoder->quantize_model_kmeans_binning_mode);
    sixel_set_kmeans_binbits_override(
        encoder->quantize_model_kmeans_binbits_override,
        encoder->quantize_model_kmeans_binbits);
    sixel_set_kmeans_mapping_mode_override(
        encoder->quantize_model_kmeans_mapping_override,
        (sixel_kmeans_mapping_mode)
            encoder->quantize_model_kmeans_mapping_mode);
    sixel_set_kmeans_softdist_mode_override(
        encoder->quantize_model_kmeans_softdist_override,
        (sixel_kmeans_softdist_mode)
            encoder->quantize_model_kmeans_softdist_mode);
    sixel_set_kmeans_autoratio_override(
        encoder->quantize_model_kmeans_autoratio_override,
        encoder->quantize_model_kmeans_autoratio);
    sixel_set_kmeans_feedback_mode_override(
        encoder->quantize_model_kmeans_feedback_override,
        (sixel_kmeans_feedback_mode)
            encoder->quantize_model_kmeans_feedback_mode);
    sixel_set_kmeans_prune_policy_override(
        encoder->quantize_model_kmeans_prune_override,
        (sixel_kmeans_prune_policy)
            encoder->quantize_model_kmeans_prune_policy);
    sixel_set_kmeans_seed_override(
        encoder->quantize_model_kmeans_seed_override,
        (uint32_t)encoder->quantize_model_kmeans_seed);
    sixel_set_kmeans_restarts_override(
        encoder->quantize_model_kmeans_restarts_override,
        encoder->quantize_model_kmeans_restarts);
    sixel_set_kmeans_iter_override(
        encoder->quantize_model_kmeans_iter_override,
        encoder->quantize_model_kmeans_iter);
    sixel_set_kmeans_iter_max_override(
        encoder->quantize_model_kmeans_iter_max_override,
        encoder->quantize_model_kmeans_iter_max);
    sixel_set_kmeans_miniter_override(
        encoder->quantize_model_kmeans_miniter_override,
        encoder->quantize_model_kmeans_miniter);
    sixel_set_kmeans_polish_iter_override(
        encoder->quantize_model_kmeans_polish_iter_override,
        encoder->quantize_model_kmeans_polish_iter);
    sixel_set_kmeans_feedback_slots_override(
        encoder->quantize_model_kmeans_feedback_slots_override,
        encoder->quantize_model_kmeans_feedback_slots);
    sixel_set_kmeans_feedback_interval_override(
        encoder->quantize_model_kmeans_feedback_interval_override,
        encoder->quantize_model_kmeans_feedback_interval);
    sixel_set_final_merge_target_factor_override(
        effective_merge_oversplit_override,
        effective_merge_oversplit);
    sixel_set_final_merge_lloyd_iterations_override(
        effective_merge_lloyd_override,
        effective_merge_lloyd);
    sixel_set_kmedoids_algo_override(
        encoder->quantize_model_kmedoids_algo_override,
        (sixel_kmedoids_algo_t)encoder->quantize_model_kmedoids_algo);
    sixel_set_kmedoids_seed_override(
        encoder->quantize_model_kmedoids_seed_override,
        (uint32_t)encoder->quantize_model_kmedoids_seed);
    sixel_set_kmedoids_iter_override(
        encoder->quantize_model_kmedoids_iter_override,
        encoder->quantize_model_kmedoids_iter);
    sixel_set_kmedoids_sample_override(
        encoder->quantize_model_kmedoids_sample_override,
        encoder->quantize_model_kmedoids_sample);
    sixel_set_kmedoids_clara_trials_override(
        encoder->quantize_model_kmedoids_clara_trials_override,
        encoder->quantize_model_kmedoids_clara_trials);
    sixel_set_kmedoids_clara_sample_override(
        encoder->quantize_model_kmedoids_clara_sample_override,
        encoder->quantize_model_kmedoids_clara_sample);
    sixel_set_kmedoids_clarans_local_override(
        encoder->quantize_model_kmedoids_clarans_local_override,
        encoder->quantize_model_kmedoids_clarans_local);
    sixel_set_kmedoids_clarans_neighbors_override(
        encoder->quantize_model_kmedoids_clarans_neighbors_override,
        encoder->quantize_model_kmedoids_clarans_neighbors);
    sixel_set_kmedoids_bandit_iter_override(
        encoder->quantize_model_kmedoids_bandit_iter_override,
        encoder->quantize_model_kmedoids_bandit_iter);
    sixel_set_kmedoids_bandit_candidates_override(
        encoder->quantize_model_kmedoids_bandit_candidates_override,
        encoder->quantize_model_kmedoids_bandit_candidates);
    sixel_set_kmedoids_bandit_batch_override(
        encoder->quantize_model_kmedoids_bandit_batch_override,
        encoder->quantize_model_kmedoids_bandit_batch);
    sixel_set_kmedoids_histbits_override(
        encoder->quantize_model_kmedoids_histbits_override,
        encoder->quantize_model_kmedoids_histbits);
    sixel_set_kmedoids_point_budget_override(
        encoder->quantize_model_kmedoids_point_budget_override,
        encoder->quantize_model_kmedoids_point_budget);
    sixel_set_kmedoids_rare_keep_override(
        encoder->quantize_model_kmedoids_rare_keep_override,
        encoder->quantize_model_kmedoids_rare_keep);
    sixel_set_kmedoids_prune_mass_override(
        encoder->quantize_model_kmedoids_prune_mass_override,
        encoder->quantize_model_kmedoids_prune_mass);
    sixel_set_kmedoids_auction_override(
        encoder->quantize_model_kmedoids_auction_override,
        encoder->quantize_model_kmedoids_auction);
    sixel_set_kmedoids_auction_shortlist_override(
        encoder->quantize_model_kmedoids_auction_shortlist_override,
        encoder->quantize_model_kmedoids_auction_shortlist);
    sixel_set_kcenter_algo_override(
        encoder->quantize_model_kcenter_algo_override,
        (sixel_kcenter_algo_t)encoder->quantize_model_kcenter_algo);
    sixel_set_kcenter_profile_override(
        encoder->quantize_model_kcenter_profile_override,
        (sixel_kcenter_profile_t)encoder->quantize_model_kcenter_profile);
    sixel_set_kcenter_seed_override(
        encoder->quantize_model_kcenter_seed_override,
        (uint32_t)encoder->quantize_model_kcenter_seed);
    sixel_set_kcenter_restarts_override(
        encoder->quantize_model_kcenter_restarts_override,
        encoder->quantize_model_kcenter_restarts);
    sixel_set_kcenter_init_seeds_override(
        encoder->quantize_model_kcenter_init_seeds_override,
        encoder->quantize_model_kcenter_init_seeds);
    sixel_set_kcenter_iter_override(
        encoder->quantize_model_kcenter_iter_override,
        encoder->quantize_model_kcenter_iter);
    sixel_set_kcenter_histbits_override(
        encoder->quantize_model_kcenter_histbits_override,
        encoder->quantize_model_kcenter_histbits);
    sixel_set_kcenter_point_budget_override(
        encoder->quantize_model_kcenter_point_budget_override,
        encoder->quantize_model_kcenter_point_budget);
    sixel_set_kcenter_auto_policy_override(
        encoder->quantize_model_kcenter_auto_policy_override,
        (sixel_kcenter_auto_policy_t)
            encoder->quantize_model_kcenter_auto_policy);
    sixel_set_kcenter_auto_fft_threshold_override(
        encoder->quantize_model_kcenter_auto_fft_threshold_override,
        encoder->quantize_model_kcenter_auto_fft_threshold);
    sixel_set_kcenter_space_policy_override(
        encoder->quantize_model_kcenter_space_policy_override,
        (sixel_kcenter_space_policy_t)
            encoder->quantize_model_kcenter_space_policy);
    sixel_set_kcenter_candidate_policy_override(
        encoder->quantize_model_kcenter_candidate_policy_override,
        (sixel_kcenter_candidate_policy_t)
            encoder->quantize_model_kcenter_candidate_policy);
    sixel_set_kcenter_rare_keep_override(
        encoder->quantize_model_kcenter_rare_keep_override,
        encoder->quantize_model_kcenter_rare_keep);
    sixel_set_kcenter_budget_policy_override(
        encoder->quantize_model_kcenter_budget_policy_override,
        (sixel_kcenter_budget_policy_t)
            encoder->quantize_model_kcenter_budget_policy);
    sixel_set_kcenter_budget_scale_override(
        encoder->quantize_model_kcenter_budget_scale_override,
        encoder->quantize_model_kcenter_budget_scale);
    sixel_set_kcenter_swap_topk_override(
        encoder->quantize_model_kcenter_swap_topk_override,
        encoder->quantize_model_kcenter_swap_topk);
    sixel_set_kcenter_swap_update_override(
        encoder->quantize_model_kcenter_swap_update_override,
        (sixel_kcenter_swap_update_t)
            encoder->quantize_model_kcenter_swap_update);
    sixel_set_kcenter_swap_patience_override(
        encoder->quantize_model_kcenter_swap_patience_override,
        encoder->quantize_model_kcenter_swap_patience);
    sixel_set_kcenter_swap_min_gain_override(
        encoder->quantize_model_kcenter_swap_min_gain_override,
        encoder->quantize_model_kcenter_swap_min_gain);
    sixel_set_kcenter_prune_mass_override(
        encoder->quantize_model_kcenter_prune_mass_override,
        encoder->quantize_model_kcenter_prune_mass);
    status = sixel_dither_initialize(*dither,
                                     palette_pixels,
                                     sixel_frame_get_width(palette_frame),
                                     sixel_frame_get_height(palette_frame),
                                     palette_pixelformat,
                                     effective_method_for_largest,
                                     encoder->method_for_rep,
                                     encoder->quality_mode);
    sixel_set_kmeans_init_type_override(0, SIXEL_PALETTE_KMEANS_INIT_AUTO);
    sixel_set_kmeans_threshold_override(0, 0.125);
    sixel_set_kmeans_binning_mode_override(
        0,
        SIXEL_PALETTE_KMEANS_BINNING_AUTO);
    sixel_set_kmeans_binbits_override(0, 6u);
    sixel_set_kmeans_mapping_mode_override(
        0,
        SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM);
    sixel_set_kmeans_softdist_mode_override(
        0,
        SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR);
    sixel_set_kmeans_autoratio_override(0, 32u);
    sixel_set_kmeans_seed_override(0, 0u);
    sixel_set_kmeans_restarts_override(0, 1u);
    sixel_set_kmeans_iter_override(0, 0u);
    sixel_set_kmeans_iter_max_override(0, 20u);
    sixel_set_kmeans_miniter_override(0, 0u);
    sixel_set_kmeans_polish_iter_override(0, 0u);
    sixel_set_kmeans_feedback_mode_override(
        0,
        SIXEL_PALETTE_KMEANS_FEEDBACK_OFF);
    sixel_set_kmeans_prune_policy_override(
        0,
        SIXEL_PALETTE_KMEANS_PRUNE_AUTO);
    sixel_set_kmeans_feedback_slots_override(0, 1u);
    sixel_set_kmeans_feedback_interval_override(0, 1u);
    sixel_set_final_merge_target_factor_override(0, 1.81);
    sixel_set_final_merge_lloyd_iterations_override(0, 3u);
    sixel_set_kmedoids_algo_override(
        0,
        SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO);
    sixel_set_kmedoids_seed_override(0, 1u);
    sixel_set_kmedoids_iter_override(0, 0u);
    sixel_set_kmedoids_sample_override(0, 0u);
    sixel_set_kmedoids_clara_trials_override(0, 0u);
    sixel_set_kmedoids_clara_sample_override(0, 0u);
    sixel_set_kmedoids_clarans_local_override(0, 0u);
    sixel_set_kmedoids_clarans_neighbors_override(0, 0u);
    sixel_set_kmedoids_bandit_iter_override(0, 0u);
    sixel_set_kmedoids_bandit_candidates_override(0, 0u);
    sixel_set_kmedoids_bandit_batch_override(0, 0u);
    sixel_set_kmedoids_histbits_override(0, 0u);
    sixel_set_kmedoids_point_budget_override(0, 0u);
    sixel_set_kmedoids_rare_keep_override(0, 0u);
    sixel_set_kmedoids_prune_mass_override(0, 0.0);
    sixel_set_kmedoids_auction_override(0, 0);
    sixel_set_kmedoids_auction_shortlist_override(0, 4u);
    sixel_set_kcenter_algo_override(
        0,
        SIXEL_PALETTE_KCENTER_ALGO_AUTO);
    sixel_set_kcenter_profile_override(
        0,
        SIXEL_PALETTE_KCENTER_PROFILE_LEGACY);
    sixel_set_kcenter_seed_override(0, 1u);
    sixel_set_kcenter_restarts_override(0, 1u);
    sixel_set_kcenter_init_seeds_override(0, 1u);
    sixel_set_kcenter_iter_override(0, 16u);
    sixel_set_kcenter_histbits_override(0, 5u);
    sixel_set_kcenter_point_budget_override(0, 0u);
    sixel_set_kcenter_auto_policy_override(
        0,
        SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY);
    sixel_set_kcenter_auto_fft_threshold_override(0, 2048u);
    sixel_set_kcenter_space_policy_override(
        0,
        SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY);
    sixel_set_kcenter_candidate_policy_override(
        0,
        SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY);
    sixel_set_kcenter_rare_keep_override(0, 0u);
    sixel_set_kcenter_budget_policy_override(
        0,
        SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY);
    sixel_set_kcenter_budget_scale_override(0, 1.0);
    sixel_set_kcenter_swap_topk_override(0, 1u);
    sixel_set_kcenter_swap_update_override(
        0,
        SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL);
    sixel_set_kcenter_swap_patience_override(0, 0u);
    sixel_set_kcenter_swap_min_gain_override(0, 0.0);
    sixel_set_kcenter_prune_mass_override(0, 0.995);
    if (SIXEL_FAILED(status)) {
        sixel_dither_unref(*dither);
        goto end;
    }

    if (clustering_colorspace != working_colorspace) {
        status = sixel_encoder_convert_palette_colorspace(
            (*dither)->palette,
            clustering_colorspace,
            working_colorspace);
        if (SIXEL_FAILED(status)) {
            sixel_dither_unref(*dither);
            goto end;
        }
    }
    if (reserve_alpha_key) {
        status = sixel_encoder_attach_alpha_keycolor(encoder, *dither);
        if (SIXEL_FAILED(status)) {
            sixel_dither_unref(*dither);
            goto end;
        }
    }
    sixel_encoder_quantize_override_lock_release(
        quantize_override_lock_acquired);
    quantize_override_lock_acquired = 0;

    sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));

    status = SIXEL_OK;

end:
    if (quantize_override_lock_acquired != 0) {
        sixel_encoder_quantize_override_lock_release(
            quantize_override_lock_acquired);
        quantize_override_lock_acquired = 0;
    }
    if (cluster_frame != NULL) {
        sixel_frame_unref(cluster_frame);
        cluster_frame = NULL;
    }
    if (SIXEL_SUCCEEDED(status) && dither != NULL && *dither != NULL) {
        sixel_dither_set_lut_policy(*dither, encoder->lut_policy);
        (*dither)->lut_policy_shared_instance_override =
            encoder->lut_policy_shared_instance_override;
        (*dither)->lut_policy_shared_instance =
            encoder->lut_policy_shared_instance;
        /* pass down the user's demand for an exact palette size */
        (*dither)->force_palette = encoder->force_palette;
        if (cache_allowed
                && fixed_palette_cache_candidate != 0
                && dither_cache_hit == 0) {
            /*
             * Fixed palette sources are invariant across animation frames.
             * Cache the parsed dither object so stdin mapfiles are consumed
             * only once while later frames still observe the same palette.
             */
            sixel_encoder_clear_dither_cache(encoder);
            encoder->dither_cache = *dither;
            sixel_dither_ref(*dither);
        }
    }
    return status;
}

static SIXELSTATUS
sixel_encoder_palette_builder(void *userdata,
                              sixel_frame_t *frame,
                              sixel_dither_t **dither_out,
                              sixel_timeline_logger_t *logger)
{
    sixel_palette_builder_context_t *context;

    context = NULL;

    if (userdata == NULL || frame == NULL || dither_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    context = (sixel_palette_builder_context_t *)userdata;

    return sixel_encoder_prepare_palette(context->encoder,
                                         frame,
                                         dither_out,
                                         context->allow_cache,
                                         logger);
}

static SIXELSTATUS
sixel_encoder_apply_palette_filter(sixel_encoder_t *encoder,
                                   sixel_frame_t **frame_slot,
                                   int allow_cache,
                                   sixel_dither_t **dither_out)
{
    SIXELSTATUS status;
    sixel_palette_builder_context_t builder_context;
    sixel_filter_palette_config_t palette_config;
    sixel_filter_t *filter;
    int height;

    status = SIXEL_FALSE;
    filter = NULL;
    height = 0;

    if (encoder == NULL || frame_slot == NULL || dither_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (*frame_slot == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    builder_context.encoder = encoder;
    builder_context.allow_cache = allow_cache;
    palette_config.builder = sixel_encoder_palette_builder;
    palette_config.builder_userdata = &builder_context;
    palette_config.dither_out = dither_out;

    status = sixel_filter_factory_create_by_name(
        "palette", &palette_config, &filter);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    sixel_filter_bind_input(filter,
                            frame_slot,
                            sixel_frame_get_pixelformat(*frame_slot),
                            sixel_frame_get_colorspace(*frame_slot));

    height = sixel_frame_get_height(*frame_slot);
    if (height < 0) {
        height = 0;
    }
    sixel_filter_set_progress(filter, NULL, NULL, height);

    status = sixel_filter_run(filter, encoder->allocator, encoder->logger);

    sixel_filter_free(filter);

    return status;
}

static SIXELSTATUS
sixel_encoder_apply_lut_filter(sixel_encoder_t *encoder,
                               sixel_dither_t *dither)
{
    SIXELSTATUS status;
    sixel_filter_lookup_result_t result;
    sixel_filter_lookup_config_t lookup_config;
    sixel_filter_fhedt_config_t fhedt_config;
    sixel_filter_vptree_config_t vptree_config;
    sixel_filter_1d_eytzinger_config_t eytzinger_config;
    sixel_filter_t *filter;
    sixel_palette_t *palette;
    int policy;
    sixel_palette_entries_view_t entries_view;
    sixel_palette_float32_entries_view_t float32_view;

    status = SIXEL_FALSE;
    filter = NULL;
    palette = NULL;
    policy = SIXEL_LUT_POLICY_AUTO;
    memset(&entries_view, 0, sizeof(entries_view));
    memset(&float32_view, 0, sizeof(float32_view));
    memset(&result, 0, sizeof(result));
    memset(&lookup_config, 0, sizeof(lookup_config));
    memset(&fhedt_config, 0, sizeof(fhedt_config));
    memset(&vptree_config, 0, sizeof(vptree_config));
    memset(&eytzinger_config, 0, sizeof(eytzinger_config));

    if (encoder == NULL || dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    palette = dither->palette;
    if (palette == NULL || palette->vtbl == NULL ||
        palette->vtbl->get_entries == NULL ||
        palette->vtbl->get_entries_float32 == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = palette->vtbl->get_entries(palette, &entries_view);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = palette->vtbl->get_entries_float32(palette, &float32_view);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (entries_view.entries == NULL || entries_view.depth <= 0
            || entries_view.entry_count == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }

    policy = dither->lut_policy;
    if (policy != SIXEL_LUT_POLICY_FHEDT
            && policy != SIXEL_LUT_POLICY_VPTREE
            && policy != SIXEL_LUT_POLICY_EYTZINGER) {
        return SIXEL_OK;
    }

    lookup_config.palette = entries_view.entries;
    lookup_config.palette_float = float32_view.entries;
    lookup_config.depth = entries_view.depth;
    lookup_config.float_depth = float32_view.depth;
    lookup_config.ncolors = (int)entries_view.entry_count;
    lookup_config.lut_policy = policy;
    lookup_config.pixelformat = dither->pixelformat;
    lookup_config.reuse_policy = dither->lookup_policy;

    if (policy == SIXEL_LUT_POLICY_FHEDT) {
        fhedt_config.lookup_config = lookup_config;
        fhedt_config.result_out = &result;
        status = sixel_filter_factory_create_by_kind(
            SIXEL_FILTER_KIND_FHEDT,
            &fhedt_config,
            &filter);
    } else if (policy == SIXEL_LUT_POLICY_VPTREE) {
        vptree_config.lookup_config = lookup_config;
        vptree_config.result_out = &result;
        status = sixel_filter_factory_create_by_kind(
            SIXEL_FILTER_KIND_VPTREE,
            &vptree_config,
            &filter);
    } else {
        eytzinger_config.lookup_config = lookup_config;
        eytzinger_config.result_out = &result;
        status = sixel_filter_factory_create_by_kind(
            SIXEL_FILTER_KIND_EYTZINGER,
            &eytzinger_config,
            &filter);
    }
    if (SIXEL_FAILED(status)) {
        return status;
    }

    sixel_filter_set_progress(filter, NULL, NULL, 1);
    status = sixel_filter_run(filter,
                              encoder->allocator,
                              encoder->logger);
    if (SIXEL_SUCCEEDED(status) && result.policy != NULL) {
        if (dither->lookup_policy != NULL
                && dither->lookup_policy != result.policy) {
            dither->lookup_policy->vtbl->unref(dither->lookup_policy);
        }
        dither->lookup_policy = result.policy;
    }

    sixel_filter_free(filter);

    return status;
}


static void
sixel_debug_print_palette(
    sixel_dither_t /* in */ *dither /* dithering object */
)
{
    sixel_palette_entries_view_t palette_view;
    int i;

    memset(&palette_view, 0, sizeof(palette_view));
    if (dither == NULL) {
        return;
    }

    if (dither->palette == NULL || dither->palette->vtbl == NULL ||
            dither->palette->vtbl->get_entries == NULL) {
        return;
    }
    if (SIXEL_FAILED(dither->palette->vtbl->get_entries(dither->palette,
                                                        &palette_view))
            || palette_view.entries == NULL || palette_view.depth < 3) {
        return;
    }

    fprintf(stderr, "palette:\n");
    for (i = 0; i < (int)palette_view.entry_count;
            ++i) {
        fprintf(stderr, "%d: #%02x%02x%02x\n", i,
                palette_view.entries[i * 3 + 0],
                palette_view.entries[i * 3 + 1],
                palette_view.entries[i * 3 + 2]);
    }
}

/*
 * Compute the byte count for a source frame payload.
 * Packed G1/G2/G4 and PAL1/PAL2/PAL4 formats use bit-packed rows while
 * all other formats are addressed as width*height*depth bytes.
 */
static SIXELSTATUS
sixel_encoder_compute_frame_buffer_bytes(
    int width,
    int height,
    int pixelformat,
    size_t *byte_count_out)
{
    size_t row_bits;
    size_t row_bytes;
    size_t pixel_total;
    int depth;

    row_bits = 0u;
    row_bytes = 0u;
    pixel_total = 0u;
    depth = 0;

    if (byte_count_out == NULL || width <= 0 || height <= 0) {
        return SIXEL_BAD_INPUT;
    }
    *byte_count_out = 0u;

    depth = sixel_helper_compute_depth(pixelformat);
    if (depth <= 0) {
        return SIXEL_BAD_INPUT;
    }

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_G1:
    case SIXEL_PIXELFORMAT_PAL1:
        row_bits = (size_t)width;
        break;
    case SIXEL_PIXELFORMAT_G2:
    case SIXEL_PIXELFORMAT_PAL2:
        if ((size_t)width > SIZE_MAX / 2u) {
            return SIXEL_BAD_INPUT;
        }
        row_bits = (size_t)width * 2u;
        break;
    case SIXEL_PIXELFORMAT_G4:
    case SIXEL_PIXELFORMAT_PAL4:
        if ((size_t)width > SIZE_MAX / 4u) {
            return SIXEL_BAD_INPUT;
        }
        row_bits = (size_t)width * 4u;
        break;
    default:
        if ((size_t)width > SIZE_MAX / (size_t)height) {
            return SIXEL_BAD_INPUT;
        }
        pixel_total = (size_t)width * (size_t)height;
        if (pixel_total > SIZE_MAX / (size_t)depth) {
            return SIXEL_BAD_INPUT;
        }
        *byte_count_out = pixel_total * (size_t)depth;
        return SIXEL_OK;
    }

    row_bytes = (row_bits + 7u) / 8u;
    if (row_bytes > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INPUT;
    }
    *byte_count_out = row_bytes * (size_t)height;

    return SIXEL_OK;
}


static SIXELSTATUS
sixel_encoder_output_without_macro(
    sixel_frame_t       /* in */ *frame,
    sixel_dither_t      /* in */ *dither,
    sixel_output_t      /* in */ *output,
    sixel_encoder_t     /* in */ *encoder,
    int                  frame_no,
    int                  loop_no,
    int                  delay)
{
    SIXELSTATUS status = SIXEL_OK;
    unsigned char *p;
    int depth;
    enum { message_buffer_size = 2048 };
    char message[message_buffer_size];
    int nwrite;
    int dulation;
    unsigned int remaining_delay;
    unsigned int target_delay;
    unsigned char *pixbuf;
    int width = 0;
    int height = 0;
    int pixelformat = 0;
    int multiframe;
    size_t size;
    int frame_colorspace = SIXEL_COLORSPACE_GAMMA;
    int output_colorspace;
    sixel_encoding_planner_t *planner;
    int quantize_animation_enabled;
    int capture_palette_only;

    p = NULL;
    output_colorspace = SIXEL_COLORSPACE_GAMMA;
    if (encoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_without_macro: encoder object is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    planner = &encoder->planner;
    quantize_animation_enabled = 0;
    capture_palette_only = 0;
    output_colorspace = encoder->output_colorspace;

    if (encoder->color_option == SIXEL_COLOR_OPTION_DEFAULT) {
        quantize_animation_enabled =
            sixel_encoder_quantize_animation_enabled_for_frame(
                encoder,
                frame);
        if (encoder->force_palette || quantize_animation_enabled) {
            /* Keep palette slots stable when forced or animation-locked. */
            sixel_dither_set_optimize_palette(dither, 0);
        } else {
            sixel_dither_set_optimize_palette(dither, 1);
        }
    }

    pixelformat = sixel_frame_get_pixelformat(frame);
    frame_colorspace = sixel_frame_get_colorspace(frame);
    status = sixel_encoder_set_encoder_core_format(output,
                                             pixelformat,
                                             frame_colorspace,
                                             output_colorspace);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_dither_set_pixelformat(dither, pixelformat);
    depth = sixel_helper_compute_depth(pixelformat);
    if (depth < 0) {
        status = SIXEL_LOGIC_ERROR;
        nwrite = sixel_compat_snprintf(
            message,
            sizeof(message),
            "sixel_encoder_output_without_macro: "
            "sixel_helper_compute_depth(%08x) failed.",
            pixelformat);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        goto end;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    multiframe = sixel_frame_get_multiframe(frame);
    status = sixel_encoder_compute_frame_buffer_bytes(width,
                                                      height,
                                                      pixelformat,
                                                      &size);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_without_macro: "
            "invalid source frame size.");
        goto end;
    }

    sixel_encoder_log_stage(encoder,
                            frame,
                            "encode",
                            "worker",
                            "start",
                            "size=%dx%d fmt=%08x dst_cs=%d",
                            width,
                            height,
                            pixelformat,
                            output_colorspace);

    p = (unsigned char *)sixel_allocator_malloc(encoder->allocator, size);
    if (p == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_without_macro: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    if (delay > 0 && !encoder->fignore_delay && !encoder->fstatic) {
        sixel_trace_topic_message(
            "lifecycle",
            "frame delay check: frame_no=%d loop_no=%d "
            "delay_cs=%d ignore=%d static=%d",
            frame_no,
            loop_no,
            delay,
            encoder->fignore_delay,
            encoder->fstatic);
        remaining_delay = sixel_encoder_compute_remaining_delay_usec(
            output,
            delay,
            &dulation,
            &target_delay);
        sixel_trace_topic_message(
            "lifecycle",
            "frame delay timing: frame_no=%d loop_no=%d "
            "measured_usec=%d target_usec=%u",
            frame_no,
            loop_no,
            dulation,
            target_delay);
        if (remaining_delay > 0U) {
            sixel_trace_topic_message(
                "lifecycle",
                "frame delay sleep: frame_no=%d loop_no=%d sleep_usec=%u",
                frame_no,
                loop_no,
                remaining_delay);
            status = sixel_encoder_wait_delay_with_cancel(encoder,
                                                          remaining_delay);
            if (status != SIXEL_OK) {
                goto end;
            }
        }
    }

    pixbuf = sixel_frame_get_pixels(frame);
    memcpy(p, pixbuf, size);

    if (sixel_encoder_is_cancel_requested(encoder) != 0) {
        goto end;
    }

    capture_palette_only =
        encoder->capture_quantized && encoder->palette_output != NULL;
    if (encoder->capture_quantized && !capture_palette_only) {
        sixel_encoder_update_dither_frame_context(dither,
                                                  frame_no,
                                                  loop_no,
                                                  multiframe);
        status = sixel_encoder_capture_quantized(encoder,
                                                 dither,
                                                 p,
                                                 size,
                                                 width,
                                                 height,
                                                 pixelformat,
                                                 frame_colorspace,
                                                 output_colorspace);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (planner != NULL && dither != NULL) {
        dither->pipeline_pin_threads = planner->pipeline_pin_threads;
    }
    sixel_encoder_update_dither_frame_context(dither,
                                              frame_no,
                                              loop_no,
                                              multiframe);
    sixel_encoder_bind_frame_transparent_mask(dither, frame);
    status = sixel_encode(p, width, height, depth, dither, output);
    if (status != SIXEL_OK) {
        goto end;
    }
    if (capture_palette_only) {
        status = sixel_encoder_capture_quantized_palette_only(
            encoder,
            dither,
            width,
            height,
            output_colorspace);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

end:
    if (SIXEL_SUCCEEDED(status)) {
        sixel_encoder_log_stage(encoder,
                                frame,
                                "encode",
                                "worker",
                                "finish",
                                "size=%dx%d",
                                width,
                                height);
    }
    if (output != NULL) {
        (void)sixel_encoder_set_encoder_core_format(output,
                                              pixelformat,
                                              frame_colorspace,
                                              output_colorspace);
    }
    sixel_allocator_free(encoder->allocator, p);

    return status;
}


static SIXELSTATUS
sixel_encoder_output_with_macro(
    sixel_frame_t   /* in */ *frame,
    sixel_dither_t  /* in */ *dither,
    sixel_output_t  /* in */ *output,
    sixel_encoder_t /* in */ *encoder,
    int              frame_no,
    int              loop_no,
    int              delay)
{
    SIXELSTATUS status = SIXEL_OK;
    enum { message_buffer_size = 256 };
    char buffer[message_buffer_size];
    int nwrite;
    int dulation;
    unsigned int remaining_delay;
    unsigned int target_delay;
    int width;
    int height;
    int pixelformat;
    int multiframe;
    int depth;
    size_t size = 0;
    int frame_colorspace = SIXEL_COLORSPACE_GAMMA;
    int output_colorspace;
    unsigned char *converted = NULL;
    sixel_encoding_planner_t *planner;
    sixel_allocator_t *allocator;

    if (frame == NULL || output == NULL || encoder == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    allocator = encoder->allocator;
    planner = &encoder->planner;
    output_colorspace = encoder->output_colorspace;

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    multiframe = sixel_frame_get_multiframe(frame);
    pixelformat = sixel_frame_get_pixelformat(frame);
    depth = sixel_helper_compute_depth(pixelformat);
    if (depth < 0) {
        status = SIXEL_LOGIC_ERROR;
        sixel_helper_set_additional_message(
            "sixel_encoder_output_with_macro: "
            "sixel_helper_compute_depth() failed.");
        goto end;
    }

    frame_colorspace = sixel_frame_get_colorspace(frame);
    status = sixel_encoder_compute_frame_buffer_bytes(width,
                                                      height,
                                                      pixelformat,
                                                      &size);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_with_macro: "
            "invalid source frame size.");
        goto end;
    }
    converted = (unsigned char *)sixel_allocator_malloc(
        allocator, size);
    if (converted == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_with_macro: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    memcpy(converted, sixel_frame_get_pixels(frame), size);
    status = sixel_encoder_set_encoder_core_format(output,
                                             pixelformat,
                                             frame_colorspace,
                                             output_colorspace);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (loop_no == 0) {
        if (encoder->macro_number >= 0) {
            nwrite = sixel_compat_snprintf(
                buffer,
                sizeof(buffer),
                "\033P%d;0;1!z",
                encoder->macro_number);
        } else {
            nwrite = sixel_compat_snprintf(
                buffer,
                sizeof(buffer),
                "\033P%d;0;1!z",
                frame_no);
        }
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: command format failed.");
            goto end;
        }
        nwrite = sixel_write_callback(buffer,
                                      (int)strlen(buffer),
                                      &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: "
                "sixel_write_callback() failed.");
            goto end;
        }
        if (planner != NULL && dither != NULL) {
            dither->pipeline_pin_threads =
                planner->pipeline_pin_threads;
        }
        sixel_encoder_update_dither_frame_context(dither,
                                                  frame_no,
                                                  loop_no,
                                                  multiframe);
        sixel_encoder_bind_frame_transparent_mask(dither, frame);
        status = sixel_encode(converted,
                              width,
                              height,
                              depth,
                              dither,
                              output);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        nwrite = sixel_write_callback("\033\\", 2, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: "
                "sixel_write_callback() failed.");
            goto end;
        }
    }
    if (encoder->macro_number < 0) {
        nwrite = sixel_compat_snprintf(
            buffer,
            sizeof(buffer),
            "\033[%d*z",
            frame_no);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: command format failed.");
        }
        nwrite = sixel_write_callback(buffer,
                                      (int)strlen(buffer),
                                      &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: "
                "sixel_write_callback() failed.");
            goto end;
        }
        if (delay > 0 && !encoder->fignore_delay && !encoder->fstatic) {
            sixel_trace_topic_message(
                "lifecycle",
                "frame delay check: frame_no=%d loop_no=%d "
                "delay_cs=%d ignore=%d static=%d",
                frame_no,
                loop_no,
                delay,
                encoder->fignore_delay,
                encoder->fstatic);
            remaining_delay = sixel_encoder_compute_remaining_delay_usec(
                output,
                delay,
                &dulation,
                &target_delay);
            sixel_trace_topic_message(
                "lifecycle",
                "frame delay timing: frame_no=%d loop_no=%d "
                "measured_usec=%d target_usec=%u",
                frame_no,
                loop_no,
                dulation,
                target_delay);
            if (remaining_delay > 0U) {
                sixel_trace_topic_message(
                    "lifecycle",
                    "frame delay sleep: frame_no=%d loop_no=%d sleep_usec=%u",
                    frame_no,
                    loop_no,
                    remaining_delay);
                status = sixel_encoder_wait_delay_with_cancel(encoder,
                                                              remaining_delay);
                if (status != SIXEL_OK) {
                    goto end;
                }
            }
        }
    }

end:
    (void)sixel_encoder_set_encoder_core_format(output,
                                          pixelformat,
                                          frame_colorspace,
                                          output_colorspace);
    sixel_allocator_free(allocator, converted);

    return status;
}


static SIXELSTATUS
sixel_encoder_encode_frame(
    sixel_encoder_t *encoder,
    sixel_frame_t   *frame,
    sixel_output_t  *output,
    sixel_encoder_frame_handoff_meta_t const *metadata)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_encode_dag_context_t context;
    sixel_encode_dag_node_t nodes[SIXEL_DAG_NODE_COUNT];
    sixel_encoding_planner_t *planner;
    int target_pixelformat;
    int palette_ready;
    int clip_active;
    int current_pixelformat;
    int current_colorspace;

    if (encoder == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(&context, 0, sizeof(context));
    context.encoder = encoder;
    context.frame = frame;
    context.output = output;
    context.status = SIXEL_FALSE;
    context.dither = NULL;
    context.height = 0;
    context.is_animation = 0;
    context.nwrite = 0;
    context.fn_write = sixel_write_callback;
    context.write_callback = sixel_write_callback;
    context.scroll_callback = sixel_write_callback;
    context.write_priv = &encoder->outfd;
    context.scroll_priv = &encoder->outfd;
    context.async_dither = NULL;
    context.palette_job_started = 0;
    context.palette_job_initialized = 0;
    context.palette_ready = 0;
    context.planner = NULL;
    context.clip_active = 0;
    sixel_encoder_filter_plan_init(&context.pre_plan);
    sixel_encoder_filter_plan_init(&context.post_plan);
    memset(&context.resize_config, 0, sizeof(context.resize_config));
    memset(&context.clip_config, 0, sizeof(context.clip_config));
    memset(&context.colors_config, 0, sizeof(context.colors_config));
    memset(&context.gradient_config, 0, sizeof(context.gradient_config));
    memset(&context.dither_config, 0, sizeof(context.dither_config));
    context.current_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    context.current_colorspace = SIXEL_COLORSPACE_GAMMA;
    if (metadata != NULL) {
        context.frame_no = metadata->frame_no;
        context.loop_no = metadata->loop_no;
        context.delay = metadata->delay;
        context.multiframe = metadata->multiframe;
        /*
         * Frame metadata can be carried out-of-band in pipeline mode.
         * Mirror it back to the frame object so downstream helpers that read
         * frame state directly observe the same timeline context.
         */
        sixel_frame_set_frame_no(frame, context.frame_no);
        sixel_frame_set_loop_count(frame, context.loop_no);
        sixel_frame_set_delay(frame, context.delay);
        sixel_frame_set_multiframe(frame, context.multiframe);
    } else {
        context.frame_no = sixel_frame_get_frame_no(frame);
        context.loop_no = sixel_frame_get_loop_no(frame);
        context.delay = sixel_frame_get_delay(frame);
        context.multiframe = sixel_frame_get_multiframe(frame);
    }
    if (encoder != NULL && encoder->logger != NULL) {
        sixel_timeline_logger_set_frame_context(encoder->logger,
                                       context.frame_no,
                                       context.loop_no,
                                       context.multiframe);
    }

    if (encoder != NULL) {
        /*
         * Hold a reference while the planner and filters manipulate encoder
         * state.  The caller may not have incremented the count, so balance
         * the release in the common cleanup path at the end of this
         * function.
         */
        sixel_encoder_ref(encoder);
    }
    if (encoder != NULL) {
        context.planner = &encoder->planner;
    }
    planner = context.planner;
    if (planner != NULL) {
        sixel_encoding_planner_reset_for_frame(planner);
    }

    status = sixel_encoder_promote_pal8_transparent_for_geometry(
        encoder,
        context.frame);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    /*
     * Build the thread allocation plan up front so palette sampling does not
     * spawn extra workers when resize/clip/colorspace conversion already have
     * work to do on the main path.
     */
    if (planner != NULL) {
        sixel_encoding_planner_plan(planner, encoder, context.frame);
        target_pixelformat = planner->working_pixelformat;
    } else {
        target_pixelformat = sixel_encoder_pixelformat_for_colorspace(
            encoder->working_colorspace,
            encoder->prefer_float32);
    }

    current_pixelformat = sixel_frame_get_pixelformat(context.frame);
    current_colorspace = sixel_frame_get_colorspace(context.frame);

    palette_ready = sixel_encoding_palette_job_ready(encoder,
                                                     planner,
                                                     context.frame);
    if (planner != NULL) {
        sixel_encoding_planner_replan(planner,
                                      encoder,
                                      context.frame,
                                      palette_ready);
    }
    clip_active = (planner != NULL) ? planner->clip_active
        : (encoder->clipwidth > 0 && encoder->clipheight > 0);
    if (encoder->verbose) {
        sixel_encoding_planner_dump(planner,
                                    encoder,
                                    context.frame,
                                    palette_ready);
    }

    context.target_pixelformat = target_pixelformat;
    context.palette_ready = palette_ready;
    context.clip_active = clip_active;
    context.current_pixelformat = current_pixelformat;
    context.current_colorspace = current_colorspace;

    /*
     * DAG layout:
     *   load -> palette_launch -> palette_collect -> dither -> output
     *     \\-> preplan --------^
     */
    nodes[SIXEL_DAG_NODE_LOAD].label = "load";
    nodes[SIXEL_DAG_NODE_LOAD].deps = 0u;
    nodes[SIXEL_DAG_NODE_LOAD].done = 0u;
    nodes[SIXEL_DAG_NODE_LOAD].run = sixel_encode_dag_node_load;

    nodes[SIXEL_DAG_NODE_PALETTE_LAUNCH].label = "palette_launch";
    nodes[SIXEL_DAG_NODE_PALETTE_LAUNCH].deps =
        (1u << SIXEL_DAG_NODE_LOAD);
    nodes[SIXEL_DAG_NODE_PALETTE_LAUNCH].done = 0u;
    nodes[SIXEL_DAG_NODE_PALETTE_LAUNCH].run =
        sixel_encode_dag_node_palette_launch;

    nodes[SIXEL_DAG_NODE_PREPLAN].label = "preplan";
    nodes[SIXEL_DAG_NODE_PREPLAN].deps =
        (1u << SIXEL_DAG_NODE_LOAD);
    nodes[SIXEL_DAG_NODE_PREPLAN].done = 0u;
    nodes[SIXEL_DAG_NODE_PREPLAN].run = sixel_encode_dag_node_preplan;

    nodes[SIXEL_DAG_NODE_PALETTE_COLLECT].label = "palette_collect";
    nodes[SIXEL_DAG_NODE_PALETTE_COLLECT].deps =
        (1u << SIXEL_DAG_NODE_PALETTE_LAUNCH)
        | (1u << SIXEL_DAG_NODE_PREPLAN);
    nodes[SIXEL_DAG_NODE_PALETTE_COLLECT].done = 0u;
    nodes[SIXEL_DAG_NODE_PALETTE_COLLECT].run =
        sixel_encode_dag_node_palette_collect;

    nodes[SIXEL_DAG_NODE_DITHER_PLAN].label = "dither";
    nodes[SIXEL_DAG_NODE_DITHER_PLAN].deps =
        (1u << SIXEL_DAG_NODE_PALETTE_COLLECT);
    nodes[SIXEL_DAG_NODE_DITHER_PLAN].done = 0u;
    nodes[SIXEL_DAG_NODE_DITHER_PLAN].run = sixel_encode_dag_node_dither_plan;

    nodes[SIXEL_DAG_NODE_OUTPUT].label = "output";
    nodes[SIXEL_DAG_NODE_OUTPUT].deps =
        (1u << SIXEL_DAG_NODE_DITHER_PLAN);
    nodes[SIXEL_DAG_NODE_OUTPUT].done = 0u;
    nodes[SIXEL_DAG_NODE_OUTPUT].run = sixel_encode_dag_node_output;

    status = sixel_encode_dag_run_nodes(&context,
                                        nodes,
                                        SIXEL_DAG_NODE_COUNT);
    if (SIXEL_FAILED(status)) {
        goto end;
    }


end:
    if (encoder != NULL && encoder->logger != NULL) {
        sixel_timeline_logger_clear_frame_context(encoder->logger);
    }
    sixel_encoder_filter_plan_teardown(&context.pre_plan);
    sixel_encoder_filter_plan_teardown(&context.post_plan);
    if (context.palette_job_initialized != 0) {
        if (context.palette_job_started != 0
            && context.async_dither == NULL) {
            (void)sixel_encoder_palette_job_wait(&context.palette_job,
                                                 &context.async_dither);
        }
        if (context.async_dither != NULL && context.dither == NULL) {
            sixel_dither_unref(context.async_dither);
            context.async_dither = NULL;
        }
        sixel_encoder_palette_job_dispose(&context.palette_job);
    }
    if (context.output) {
        sixel_output_unref(context.output);
    }
    if (context.dither) {
        sixel_dither_unref(context.dither);
    }
    if (encoder) {
        sixel_encoder_unref(encoder);
    }

    return status;
}


/* create encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_new(
    sixel_encoder_t     /* out */ **ppencoder, /* encoder object to be created */
    sixel_allocator_t   /* in */  *allocator)  /* allocator, null if you use
                                                  default allocator */
{
    SIXELSTATUS status = SIXEL_FALSE;
    char const *env_default_bgcolor = NULL;
    char const *env_default_ncolors = NULL;
    char const *env_prefer_float32 = NULL;
    char const *env_lookup_policy = NULL;
    char const *env_sample_target = NULL;
    int ncolors;
    long parsed_ncolors;
    char *endptr;
    int prefer_float32;
    int env_match_value;
    size_t parsed_sample_target;
    int has_sample_target;
    sixel_option_choice_result_t match_result;
    char match_detail[128];

    parsed_sample_target = 0u;
    has_sample_target = 0;

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    *ppencoder
        = (sixel_encoder_t *)sixel_allocator_malloc(allocator,
                                                    sizeof(sixel_encoder_t));
    if (*ppencoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        sixel_allocator_unref(allocator);
        goto end;
    }

    (*ppencoder)->ref                   = 1U;
    (*ppencoder)->reqcolors             = (-1);
    (*ppencoder)->palette_sample_target = 0u;
    (*ppencoder)->palette_sample_override = 0;
    (*ppencoder)->force_palette         = 0;
    (*ppencoder)->mapfile               = NULL;
    (*ppencoder)->palette_output        = NULL;
    (*ppencoder)->loader_order          = NULL;
    (*ppencoder)->loader_start_frame_no = INT_MIN;
    (*ppencoder)->loader_start_frame_no_set = 0;
    (*ppencoder)->color_option          = SIXEL_COLOR_OPTION_DEFAULT;
    (*ppencoder)->builtin_palette       = 0;
    (*ppencoder)->method_for_diffuse    = SIXEL_DIFFUSE_AUTO;
    (*ppencoder)->interframe_strategy_override = 0;
    (*ppencoder)->interframe_strategy_token
        = SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE;
    (*ppencoder)->interframe_spatial_diffuse_override = 0;
    (*ppencoder)->interframe_spatial_diffuse = SIXEL_DIFFUSE_FS;
    (*ppencoder)->interframe_noise_strength_override = 0;
    (*ppencoder)->interframe_noise_strength_u8 = 0;
    (*ppencoder)->stbn_motion_adapt_override = 0;
    (*ppencoder)->stbn_motion_adapt_enabled = 0;
    (*ppencoder)->stbn_scene_cut_reset_override = 0;
    (*ppencoder)->stbn_scene_cut_reset_enabled = 0;
    (*ppencoder)->stbn_scene_detect_override = 0;
    (*ppencoder)->stbn_scene_detect_enabled = 0;
    (*ppencoder)->stbn_alpha_guard_override = 0;
    (*ppencoder)->stbn_alpha_guard_enabled = 0;
    (*ppencoder)->stbn_perceptual_weight_override = 0;
    (*ppencoder)->stbn_perceptual_weight_enabled = 0;
    (*ppencoder)->stbn_fastpath_override = 0;
    (*ppencoder)->stbn_fastpath_enabled = 0;
    (*ppencoder)->bluenoise_strength_override = 0;
    (*ppencoder)->bluenoise_strength = 0.055f;
    (*ppencoder)->bluenoise_phase_override = 0;
    (*ppencoder)->bluenoise_phase_x = 0;
    (*ppencoder)->bluenoise_phase_y = 0;
    (*ppencoder)->bluenoise_seed_override = 0;
    (*ppencoder)->bluenoise_seed = 0;
    (*ppencoder)->bluenoise_channel_override = 0;
    (*ppencoder)->bluenoise_channel_rgb = 0;
    (*ppencoder)->bluenoise_size_override = 0;
    (*ppencoder)->bluenoise_size = 64;
    (*ppencoder)->bluenoise_gradient_factor_override = 0;
    (*ppencoder)->bluenoise_gradient_factor = 0.0f;
    (*ppencoder)->method_for_scan       = SIXEL_SCAN_AUTO;
    (*ppencoder)->method_for_largest    = SIXEL_LARGE_AUTO;
    (*ppencoder)->method_for_largest_override = 0;
    (*ppencoder)->method_for_rep        = SIXEL_REP_AUTO;
    (*ppencoder)->quality_mode          = SIXEL_QUALITY_AUTO;
    (*ppencoder)->quantize_model        = SIXEL_QUANTIZE_MODEL_AUTO;
    (*ppencoder)->quantize_model_heckbert_profile
        = SIXEL_HECKBERT_PROFILE_COMPAT;
    (*ppencoder)->quantize_model_kmeans_init_override = 0;
    (*ppencoder)->quantize_model_kmeans_init_type = SIXEL_PALETTE_KMEANS_INIT_AUTO;
    (*ppencoder)->quantize_model_kmeans_threshold_override = 0;
    (*ppencoder)->quantize_model_kmeans_threshold = 0.125;
    (*ppencoder)->quantize_model_kmeans_binning_override = 0;
    (*ppencoder)->quantize_model_kmeans_binning_mode
        = SIXEL_PALETTE_KMEANS_BINNING_AUTO;
    (*ppencoder)->quantize_model_kmeans_binbits_override = 0;
    (*ppencoder)->quantize_model_kmeans_binbits = 6u;
    (*ppencoder)->quantize_model_kmeans_mapping_override = 0;
    (*ppencoder)->quantize_model_kmeans_mapping_mode
        = SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM;
    (*ppencoder)->quantize_model_kmeans_softdist_override = 0;
    (*ppencoder)->quantize_model_kmeans_softdist_mode
        = SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR;
    (*ppencoder)->quantize_model_kmeans_autoratio_override = 0;
    (*ppencoder)->quantize_model_kmeans_autoratio = 32u;
    (*ppencoder)->quantize_model_kmeans_feedback_override = 0;
    (*ppencoder)->quantize_model_kmeans_feedback_mode
        = SIXEL_PALETTE_KMEANS_FEEDBACK_OFF;
    (*ppencoder)->quantize_model_kmeans_prune_override = 0;
    (*ppencoder)->quantize_model_kmeans_prune_policy
        = SIXEL_PALETTE_KMEANS_PRUNE_AUTO;
    (*ppencoder)->quantize_model_kmeans_seed_override = 0;
    (*ppencoder)->quantize_model_kmeans_seed = 0u;
    (*ppencoder)->quantize_model_kmeans_restarts_override = 0;
    (*ppencoder)->quantize_model_kmeans_restarts = 1u;
    (*ppencoder)->quantize_model_kmeans_iter_override = 0;
    (*ppencoder)->quantize_model_kmeans_iter = 0u;
    (*ppencoder)->quantize_model_kmeans_iter_max_override = 0;
    (*ppencoder)->quantize_model_kmeans_iter_max = 20u;
    (*ppencoder)->quantize_model_kmeans_miniter_override = 0;
    (*ppencoder)->quantize_model_kmeans_miniter = 0u;
    (*ppencoder)->quantize_model_kmeans_polish_iter_override = 0;
    (*ppencoder)->quantize_model_kmeans_polish_iter = 0u;
    (*ppencoder)->quantize_model_kmeans_feedback_slots_override = 0;
    (*ppencoder)->quantize_model_kmeans_feedback_slots = 1u;
    (*ppencoder)->quantize_model_kmeans_feedback_interval_override = 0;
    (*ppencoder)->quantize_model_kmeans_feedback_interval = 1u;
    (*ppencoder)->quantize_model_kmedoids_algo_override = 0;
    (*ppencoder)->quantize_model_kmedoids_algo
        = SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO;
    (*ppencoder)->quantize_model_kmedoids_seed_override = 0;
    (*ppencoder)->quantize_model_kmedoids_seed = 1u;
    (*ppencoder)->quantize_model_kmedoids_iter_override = 0;
    (*ppencoder)->quantize_model_kmedoids_iter = 0u;
    (*ppencoder)->quantize_model_kmedoids_sample_override = 0;
    (*ppencoder)->quantize_model_kmedoids_sample = 0u;
    (*ppencoder)->quantize_model_kmedoids_clara_trials_override = 0;
    (*ppencoder)->quantize_model_kmedoids_clara_trials = 0u;
    (*ppencoder)->quantize_model_kmedoids_clara_sample_override = 0;
    (*ppencoder)->quantize_model_kmedoids_clara_sample = 0u;
    (*ppencoder)->quantize_model_kmedoids_clarans_local_override = 0;
    (*ppencoder)->quantize_model_kmedoids_clarans_local = 0u;
    (*ppencoder)->quantize_model_kmedoids_clarans_neighbors_override = 0;
    (*ppencoder)->quantize_model_kmedoids_clarans_neighbors = 0u;
    (*ppencoder)->quantize_model_kmedoids_bandit_iter_override = 0;
    (*ppencoder)->quantize_model_kmedoids_bandit_iter = 0u;
    (*ppencoder)->quantize_model_kmedoids_bandit_candidates_override = 0;
    (*ppencoder)->quantize_model_kmedoids_bandit_candidates = 0u;
    (*ppencoder)->quantize_model_kmedoids_bandit_batch_override = 0;
    (*ppencoder)->quantize_model_kmedoids_bandit_batch = 0u;
    (*ppencoder)->quantize_model_kmedoids_histbits_override = 0;
    (*ppencoder)->quantize_model_kmedoids_histbits = 0u;
    (*ppencoder)->quantize_model_kmedoids_point_budget_override = 0;
    (*ppencoder)->quantize_model_kmedoids_point_budget = 0u;
    (*ppencoder)->quantize_model_kmedoids_rare_keep_override = 0;
    (*ppencoder)->quantize_model_kmedoids_rare_keep = 0u;
    (*ppencoder)->quantize_model_kmedoids_prune_mass_override = 0;
    (*ppencoder)->quantize_model_kmedoids_prune_mass = 0.0;
    (*ppencoder)->quantize_model_kmedoids_auction_override = 0;
    (*ppencoder)->quantize_model_kmedoids_auction = 0;
    (*ppencoder)->quantize_model_kmedoids_auction_shortlist_override = 0;
    (*ppencoder)->quantize_model_kmedoids_auction_shortlist = 4u;
    (*ppencoder)->quantize_model_kcenter_algo_override = 0;
    (*ppencoder)->quantize_model_kcenter_algo
        = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
    (*ppencoder)->quantize_model_kcenter_profile_override = 0;
    (*ppencoder)->quantize_model_kcenter_profile
        = SIXEL_PALETTE_KCENTER_PROFILE_LEGACY;
    (*ppencoder)->quantize_model_kcenter_seed_override = 0;
    (*ppencoder)->quantize_model_kcenter_seed = 1u;
    (*ppencoder)->quantize_model_kcenter_restarts_override = 0;
    (*ppencoder)->quantize_model_kcenter_restarts = 1u;
    (*ppencoder)->quantize_model_kcenter_init_seeds_override = 0;
    (*ppencoder)->quantize_model_kcenter_init_seeds = 1u;
    (*ppencoder)->quantize_model_kcenter_iter_override = 0;
    (*ppencoder)->quantize_model_kcenter_iter = 16u;
    (*ppencoder)->quantize_model_kcenter_histbits_override = 0;
    (*ppencoder)->quantize_model_kcenter_histbits = 5u;
    (*ppencoder)->quantize_model_kcenter_point_budget_override = 0;
    (*ppencoder)->quantize_model_kcenter_point_budget = 0u;
    (*ppencoder)->quantize_model_kcenter_auto_policy_override = 0;
    (*ppencoder)->quantize_model_kcenter_auto_policy
        = SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY;
    (*ppencoder)->quantize_model_kcenter_auto_fft_threshold_override = 0;
    (*ppencoder)->quantize_model_kcenter_auto_fft_threshold = 2048u;
    (*ppencoder)->quantize_model_kcenter_space_policy_override = 0;
    (*ppencoder)->quantize_model_kcenter_space_policy
        = SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY;
    (*ppencoder)->quantize_model_kcenter_candidate_policy_override = 0;
    (*ppencoder)->quantize_model_kcenter_candidate_policy
        = SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY;
    (*ppencoder)->quantize_model_kcenter_rare_keep_override = 0;
    (*ppencoder)->quantize_model_kcenter_rare_keep = 0u;
    (*ppencoder)->quantize_model_kcenter_budget_policy_override = 0;
    (*ppencoder)->quantize_model_kcenter_budget_policy
        = SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY;
    (*ppencoder)->quantize_model_kcenter_budget_scale_override = 0;
    (*ppencoder)->quantize_model_kcenter_budget_scale = 1.0;
    (*ppencoder)->quantize_model_kcenter_swap_topk_override = 0;
    (*ppencoder)->quantize_model_kcenter_swap_topk = 1u;
    (*ppencoder)->quantize_model_kcenter_swap_update_override = 0;
    (*ppencoder)->quantize_model_kcenter_swap_update
        = SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL;
    (*ppencoder)->quantize_model_kcenter_swap_patience_override = 0;
    (*ppencoder)->quantize_model_kcenter_swap_patience = 0u;
    (*ppencoder)->quantize_model_kcenter_swap_min_gain_override = 0;
    (*ppencoder)->quantize_model_kcenter_swap_min_gain = 0.0;
    (*ppencoder)->quantize_model_kcenter_prune_mass_override = 0;
    (*ppencoder)->quantize_model_kcenter_prune_mass = 0.995;
    (*ppencoder)->quantize_model_merge_override = 0;
    (*ppencoder)->quantize_model_merge_mode = SIXEL_FINAL_MERGE_AUTO;
    (*ppencoder)->quantize_model_merge_oversplit_override = 0;
    (*ppencoder)->quantize_model_merge_oversplit = 1.81;
    (*ppencoder)->quantize_model_merge_lloyd_override = 0;
    (*ppencoder)->quantize_model_merge_lloyd = 3u;
    (*ppencoder)->quantize_model_animation_mode_override = 0;
    (*ppencoder)->quantize_model_animation_mode = 0;
    (*ppencoder)->quantize_model_scene_cut_threshold_override = 0;
    (*ppencoder)->quantize_model_scene_cut_threshold
        = SIXEL_QUANTIZE_SCENE_CUT_THRESHOLD_DEFAULT;
    memset((*ppencoder)->quantize_animation_prev_palette,
           0,
           sizeof((*ppencoder)->quantize_animation_prev_palette));
    memset((*ppencoder)->quantize_animation_prev_palette_float,
           0,
           sizeof((*ppencoder)->quantize_animation_prev_palette_float));
    (*ppencoder)->quantize_animation_prev_palette_count = 0U;
    (*ppencoder)->quantize_animation_prev_palette_valid = 0;
    (*ppencoder)->quantize_animation_prev_palette_float_valid = 0;
    (*ppencoder)->quantize_animation_prev_palette_float_stride = 0;
    memset((*ppencoder)->quantize_animation_prev_probe,
           0,
           sizeof((*ppencoder)->quantize_animation_prev_probe));
    (*ppencoder)->quantize_animation_prev_probe_valid = 0;
    (*ppencoder)->quantize_animation_prev_width = 0;
    (*ppencoder)->quantize_animation_prev_height = 0;
    (*ppencoder)->final_merge_mode      = SIXEL_FINAL_MERGE_AUTO;
    (*ppencoder)->lut_policy            = SIXEL_LUT_POLICY_CERTLUT;
    (*ppencoder)->lut_policy_override   = 0;
    (*ppencoder)->lut_policy_shared_instance_override = 0;
    (*ppencoder)->lut_policy_shared_instance = 0;
    (*ppencoder)->sixel_reversible      = 0;
    (*ppencoder)->method_for_resampling = SIXEL_RES_BILINEAR;
    (*ppencoder)->loop_mode             = SIXEL_LOOP_AUTO;
    (*ppencoder)->palette_type          = SIXEL_PALETTETYPE_AUTO;
    (*ppencoder)->f8bit                 = 0;
    (*ppencoder)->has_gri_arg_limit     = 0;
    (*ppencoder)->finvert               = 0;
    (*ppencoder)->fuse_macro            = 0;
    (*ppencoder)->fdrcs                 = 0;
    (*ppencoder)->fignore_delay         = 0;
    (*ppencoder)->fstatic               = 0;
    (*ppencoder)->cell_width            = 0;
    (*ppencoder)->cell_height           = 0;
    (*ppencoder)->pixelwidth            = (-1);
    (*ppencoder)->pixelheight           = (-1);
    (*ppencoder)->percentwidth          = (-1);
    (*ppencoder)->percentheight         = (-1);
    (*ppencoder)->clipx                 = 0;
    (*ppencoder)->clipy                 = 0;
    (*ppencoder)->clipwidth             = 0;
    (*ppencoder)->clipheight            = 0;
    (*ppencoder)->clipfirst             = 0;
    (*ppencoder)->macro_number          = (-1);
    (*ppencoder)->verbose               = 0;
    (*ppencoder)->penetrate_multiplexer = 0;
    (*ppencoder)->encode_policy         = SIXEL_ENCODEPOLICY_AUTO;
    (*ppencoder)->clustering_colorspace = SIXEL_COLORSPACE_GAMMA;
    (*ppencoder)->working_colorspace    = SIXEL_COLORSPACE_GAMMA;
    (*ppencoder)->working_colorspace_set = 0;
    (*ppencoder)->clustering_colorspace_set = 0;
    (*ppencoder)->force_float32_colorspace = 0;
    (*ppencoder)->output_colorspace     = SIXEL_COLORSPACE_GAMMA;
    (*ppencoder)->prefer_float32        = 0;
    (*ppencoder)->ormode                = 0;
    (*ppencoder)->pipe_mode             = 0;
    (*ppencoder)->bgcolor               = NULL;
    (*ppencoder)->bgcolor_source        = SIXEL_LOADER_BGCOLOR_SOURCE_EXPLICIT;
    (*ppencoder)->outfd                 = STDOUT_FILENO;
    (*ppencoder)->tile_outfd            = (-1);
    (*ppencoder)->finsecure             = 0;
    (*ppencoder)->cancel_flag           = NULL;
    (*ppencoder)->cancel_function       = NULL;
    (*ppencoder)->cancel_context        = NULL;
    (*ppencoder)->dither_cache          = NULL;
    (*ppencoder)->diagnostic_dither     = NULL;
    (*ppencoder)->drcs_charset_no       = 1u;
    (*ppencoder)->drcs_mmv              = 2;
    (*ppencoder)->capture_quantized     = 0;
    (*ppencoder)->capture_pixels        = NULL;
    (*ppencoder)->capture_pixels_size   = 0;
    (*ppencoder)->capture_palette       = NULL;
    (*ppencoder)->capture_palette_size  = 0;
    (*ppencoder)->capture_pixel_bytes   = 0;
    (*ppencoder)->capture_width         = 0;
    (*ppencoder)->capture_height        = 0;
    (*ppencoder)->capture_pixelformat   = SIXEL_PIXELFORMAT_RGB888;
    (*ppencoder)->capture_colorspace    = SIXEL_COLORSPACE_GAMMA;
    (*ppencoder)->capture_ncolors       = 0;
    (*ppencoder)->capture_valid         = 0;
    (*ppencoder)->output_is_png         = 0;
    (*ppencoder)->output_png_to_stdout  = 0;
    (*ppencoder)->png_output_path       = NULL;
    (*ppencoder)->sixel_output_path     = NULL;
    (*ppencoder)->clipboard_output_active = 0;
    (*ppencoder)->clipboard_output_format[0] = '\0';
    (*ppencoder)->clipboard_output_path = NULL;
    (*ppencoder)->logger                = NULL;
    (*ppencoder)->parallel_job_id       = -1;
    (*ppencoder)->palette_job_enabled   = 1;
    sixel_encoding_planner_init(&(*ppencoder)->planner);
    (*ppencoder)->allocator             = allocator;

    prefer_float32 = 0;
    env_prefer_float32 = sixel_compat_getenv(
        SIXEL_ENCODER_PRECISION_ENVVAR);
    /*
     * $SIXEL_FLOAT32_DITHER seeds the precision preference and is later
     * overridden by the precision CLI flag when provided.
     */
    prefer_float32 = sixel_encoder_env_prefers_float32(env_prefer_float32);
    (*ppencoder)->prefer_float32 = prefer_float32;

    /*
     * $SIXEL_DITHER_LOOKUP_POLICY mirrors the -~ flag so automated wrappers
     * can seed the LUT backend before CLI overrides run.  Invalid prefixes are
     * ignored to avoid hard failures when the environment is user-provided.
     */
    match_detail[0] = '\0';
    env_lookup_policy = sixel_compat_getenv(
        SIXEL_ENCODER_LUT_POLICY_ENVVAR);
    if (env_lookup_policy != NULL) {
        match_result = sixel_option_match_choice(
            env_lookup_policy,
            g_option_choices_lut_policy,
            sizeof(g_option_choices_lut_policy)
            / sizeof(g_option_choices_lut_policy[0]),
            &env_match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            (*ppencoder)->lut_policy = env_match_value;
            (*ppencoder)->lut_policy_override = 1;
            (*ppencoder)->lut_policy_shared_instance_override = 0;
            (*ppencoder)->lut_policy_shared_instance = 0;
        }
    }

    env_sample_target = sixel_compat_getenv(
        SIXEL_ENCODER_SAMPLE_TARGET_ENVVAR);
    if (env_sample_target != NULL) {
        has_sample_target = sixel_encoder_parse_sample_target(
            env_sample_target,
            &parsed_sample_target);
        if (has_sample_target) {
            (*ppencoder)->palette_sample_target = parsed_sample_target;
            (*ppencoder)->palette_sample_override = 1;
        }
    }

    /* evaluate environment variable ${SIXEL_BGCOLOR} */
    env_default_bgcolor = sixel_compat_getenv("SIXEL_BGCOLOR");
    if (env_default_bgcolor != NULL) {
        status = sixel_parse_x_colorspec(&(*ppencoder)->bgcolor,
                                         env_default_bgcolor,
                                         allocator);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
        (*ppencoder)->bgcolor_source = SIXEL_LOADER_BGCOLOR_SOURCE_ENV;
    }

    /* evaluate environment variable ${SIXEL_COLORS} */
    env_default_ncolors = sixel_compat_getenv("SIXEL_COLORS");
    if (env_default_ncolors) {
        parsed_ncolors = 0L;
        endptr = NULL;
        errno = 0;
        parsed_ncolors = strtol(env_default_ncolors, &endptr, 10);
        if (endptr != env_default_ncolors && *endptr == '\0' &&
            errno != ERANGE && parsed_ncolors <= (long)INT_MAX) {
            ncolors = (int)parsed_ncolors;
            if (ncolors > 1 && ncolors <= SIXEL_PALETTE_MAX) {
                (*ppencoder)->reqcolors = ncolors;
            }
        }
    }

    /* success */
    status = SIXEL_OK;

    goto end;

error:
    sixel_allocator_free(allocator, *ppencoder);
    sixel_allocator_unref(allocator);
    *ppencoder = NULL;

end:
    return status;
}


/* create encoder object (deprecated version) */
SIXELAPI /* deprecated */ sixel_encoder_t *
sixel_encoder_create(void)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_encoder_t *encoder = NULL;

    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        return NULL;
    }

    return encoder;
}


/* destroy encoder object */
static void
sixel_encoder_destroy(sixel_encoder_t *encoder)
{
    sixel_allocator_t *allocator;

    if (encoder) {
        allocator = encoder->allocator;
        sixel_allocator_free(allocator, encoder->mapfile);
        sixel_allocator_free(allocator, encoder->palette_output);
        sixel_allocator_free(allocator, encoder->loader_order);
        sixel_allocator_free(allocator, encoder->bgcolor);
        sixel_dither_unref(encoder->dither_cache);
        sixel_dither_unref((sixel_dither_t *)encoder->diagnostic_dither);
        if (encoder->outfd
            && encoder->outfd != STDOUT_FILENO
            && encoder->outfd != STDERR_FILENO) {
            (void)sixel_compat_close(encoder->outfd);
        }
        if (encoder->tile_outfd >= 0
            && encoder->tile_outfd != encoder->outfd
            && encoder->tile_outfd != STDOUT_FILENO
            && encoder->tile_outfd != STDERR_FILENO) {
            (void)sixel_compat_close(encoder->tile_outfd);
        }
        if (encoder->clipboard_output_path != NULL) {
            (void)sixel_compat_unlink(encoder->clipboard_output_path);
            encoder->clipboard_output_path = NULL;
        }
        encoder->clipboard_output_active = 0;
        encoder->clipboard_output_format[0] = '\0';
        sixel_allocator_free(allocator, encoder->capture_pixels);
        sixel_allocator_free(allocator, encoder->capture_palette);
        sixel_allocator_free(allocator, encoder->png_output_path);
        sixel_allocator_free(allocator, encoder->sixel_output_path);
        sixel_allocator_free(allocator, encoder);
        sixel_allocator_unref(allocator);
    }
}


/* increase reference count of encoder object (thread-safe) */
SIXELAPI void
sixel_encoder_ref(sixel_encoder_t *encoder)
{
    if (encoder == NULL) {
        return;
    }

    (void)sixel_atomic_fetch_add_u32(&encoder->ref, 1U);
}


/* decrease reference count of encoder object (thread-safe) */
SIXELAPI void
sixel_encoder_unref(sixel_encoder_t *encoder)
{
    unsigned int previous;

    if (encoder == NULL) {
        return;
    }

    previous = sixel_atomic_fetch_sub_u32(&encoder->ref, 1U);
    if (previous == 1U) {
        sixel_encoder_destroy(encoder);
    }
}


/* set cancel state flag to encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_set_cancel_flag(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ *cancel_flag
)
{
    SIXELSTATUS status = SIXEL_OK;

    encoder->cancel_flag = cancel_flag;

    return status;
}

/* set cancel callback to encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_set_cancel_callback(
    sixel_encoder_t       /* in */ *encoder,
    sixel_cancel_function /* in */ cancel_function,
    void                  /* in */ *cancel_context
)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (encoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_set_cancel_callback: encoder is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    encoder->cancel_function = cancel_function;
    encoder->cancel_context = cancel_context;

end:
    return status;
}


static int
is_png_target(char const *path)
{
    size_t len;
    int matched;

    /*
     * Detect PNG requests from explicit prefixes or a ".png" suffix:
     *
     *   argument
     *   |
     *   v
     *   .............. . p n g
     *   ^             ^^^^^^^^^
     *   |             +-- case-insensitive suffix comparison
     *   +-- accepts the "png:" inline prefix used for stdout capture
     */

    len = 0;
    matched = 0;

    if (path == NULL) {
        return 0;
    }

    if (strncmp(path, "png:", 4) == 0) {
        return path[4] != '\0';
    }

    len = strlen(path);
    if (len >= 4) {
        matched = (tolower((unsigned char)path[len - 4]) == '.')
            && (tolower((unsigned char)path[len - 3]) == 'p')
            && (tolower((unsigned char)path[len - 2]) == 'n')
            && (tolower((unsigned char)path[len - 1]) == 'g');
    }

    return matched;
}


static char const *
png_target_payload_view(char const *argument)
{
    /*
     * Inline PNG targets split into either a prefix/payload pair or rely on
     * a simple file-name suffix:
     *
     *   +--------------+------------+-------------+
     *   | form         | payload    | destination |
     *   +--------------+------------+-------------+
     *   | png:         | -          | stdout      |
     *   | png:         | filename   | filesystem  |
     *   | *.png        | filename   | filesystem  |
     *   +--------------+------------+-------------+
     *
     * The caller only needs the payload column, so we expose it here.  When
     * the user omits the prefix we simply echo the original pointer so the
     * caller can copy the value verbatim.
     */
    if (argument == NULL) {
        return NULL;
    }
    if (strncmp(argument, "png:", 4) == 0) {
        return argument + 4;
    }

    return argument;
}

static int
sixel_encoder_threads_token_is_auto(char const *text)
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

static int
sixel_encoder_parse_threads_argument(char const *text, int *value)
{
    long parsed;
    char *endptr;

    parsed = 0L;
    endptr = NULL;

    if (text == NULL || value == NULL) {
        return 0;
    }

    if (sixel_encoder_threads_token_is_auto(text) != 0) {
        *value = 0;
        return 1;
    }

    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0' || errno == ERANGE) {
        return 0;
    }

    if (parsed < 1L || parsed > (long)INT_MAX) {
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

static int
sixel_encoder_parse_crop_geometry(char const *value,
                                  int *width,
                                  int *height,
                                  int *offset_x,
                                  int *offset_y)
{
    long parsed;
    char *endptr;
    char const *cursor;

    parsed = 0L;
    endptr = NULL;
    cursor = NULL;

    if (value == NULL || width == NULL || height == NULL ||
        offset_x == NULL || offset_y == NULL) {
        return 0;
    }

    /*
     * Crop geometry uses a fixed grammar:
     *
     *   <width>x<height>+<x>+<y>
     *
     * The parser walks each token with strtol() and validates the expected
     * delimiter so MSVCRT avoids sscanf_s() constraints.
     */
    cursor = value;
    errno = 0;
    parsed = strtol(cursor, &endptr, 10);
    if (endptr == cursor || errno == ERANGE ||
        parsed <= 0L || parsed > (long)INT_MAX) {
        return 0;
    }
    if (*endptr != 'x') {
        return 0;
    }
    *width = (int)parsed;

    cursor = endptr + 1;
    errno = 0;
    parsed = strtol(cursor, &endptr, 10);
    if (endptr == cursor || errno == ERANGE ||
        parsed <= 0L || parsed > (long)INT_MAX) {
        return 0;
    }
    if (*endptr != '+') {
        return 0;
    }
    *height = (int)parsed;

    cursor = endptr + 1;
    errno = 0;
    parsed = strtol(cursor, &endptr, 10);
    if (endptr == cursor || errno == ERANGE ||
        parsed < 0L || parsed > (long)INT_MAX) {
        return 0;
    }
    if (*endptr != '+') {
        return 0;
    }
    *offset_x = (int)parsed;

    cursor = endptr + 1;
    errno = 0;
    parsed = strtol(cursor, &endptr, 10);
    if (endptr == cursor || errno == ERANGE ||
        parsed < 0L || parsed > (long)INT_MAX) {
        return 0;
    }
    if (*endptr != '\0') {
        return 0;
    }
    *offset_y = (int)parsed;

    return 1;
}

static int
sixel_encoder_parse_dimension_value(char const *value,
                                    long *number,
                                    char const **suffix)
{
    long parsed;
    char *endptr;

    parsed = 0L;
    endptr = NULL;

    if (value == NULL || number == NULL || suffix == NULL) {
        return 0;
    }

    errno = 0;
    parsed = strtol(value, &endptr, 10);
    if (endptr == value || errno == ERANGE) {
        return 0;
    }

    *number = parsed;
    *suffix = endptr;
    return 1;
}

static SIXELSTATUS
sixel_encoder_parse_interframe_noise_strength_text(
    char const *text,
    int *strength_u8_out)
{
    char *endptr;
    double parsed;
    double scaled;

    endptr = NULL;
    parsed = 0.0;
    scaled = 0.0;
    if (text == NULL || strength_u8_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtod(text, &endptr);
    if (endptr == text ||
            endptr == NULL ||
            endptr[0] != '\0' ||
            errno != 0 ||
            parsed < 0.0 ||
            parsed > 2.0) {
        sixel_helper_set_additional_message(
            "-d stbn:strength must be in range 0.0-2.0.");
        return SIXEL_BAD_ARGUMENT;
    }

    scaled = parsed * 255.0;
    if (scaled > 255.0) {
        scaled = 255.0;
    }
    *strength_u8_out = (int)(scaled + 0.5);
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_bluenoise_strength_text(
    char const *text,
    float *strength_out)
{
    char *endptr;
    double parsed;

    endptr = NULL;
    parsed = 0.0;
    if (text == NULL || strength_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtod(text, &endptr);
    if (endptr == text ||
            endptr == NULL ||
            endptr[0] != '\0' ||
            errno != 0) {
        sixel_helper_set_additional_message(
            "-d bluenoise:strength must be a floating point value.");
        return SIXEL_BAD_ARGUMENT;
    }

    *strength_out = (float)parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_bluenoise_gradient_factor_text(
    char const *text,
    float *gradient_factor_out)
{
    char *endptr;
    double parsed;

    endptr = NULL;
    parsed = 0.0;
    if (text == NULL || gradient_factor_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtod(text, &endptr);
    if (endptr == text ||
            endptr == NULL ||
            endptr[0] != '\0' ||
            errno != 0) {
        sixel_helper_set_additional_message(
            "-d bluenoise:gradient_factor must be a floating point value.");
        return SIXEL_BAD_ARGUMENT;
    }

    *gradient_factor_out = (float)parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_bluenoise_seed_text(char const *text, int *seed_out)
{
    long parsed;
    char *endptr;

    parsed = 0L;
    endptr = NULL;
    if (text == NULL || seed_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (endptr == text ||
            endptr == NULL ||
            endptr[0] != '\0' ||
            errno == ERANGE ||
            parsed > (long)INT_MAX ||
            parsed < (long)INT_MIN) {
        sixel_helper_set_additional_message(
            "-d bluenoise:seed must be a 32-bit signed integer.");
        return SIXEL_BAD_ARGUMENT;
    }

    *seed_out = (int)parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_bluenoise_phase_text(char const *text,
                                         int *phase_x_out,
                                         int *phase_y_out)
{
    char *endptr;
    char const *comma;
    long parsed_x;
    long parsed_y;

    endptr = NULL;
    comma = NULL;
    parsed_x = 0L;
    parsed_y = 0L;
    if (text == NULL || phase_x_out == NULL || phase_y_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    comma = strchr(text, ',');
    if (comma == NULL) {
        sixel_helper_set_additional_message(
            "-d bluenoise:phase must be in form X,Y.");
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed_x = strtol(text, &endptr, 10);
    if (endptr == text ||
            endptr != comma ||
            errno == ERANGE ||
            parsed_x > (long)INT_MAX ||
            parsed_x < (long)INT_MIN) {
        sixel_helper_set_additional_message(
            "-d bluenoise:phase must be in form X,Y.");
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed_y = strtol(comma + 1, &endptr, 10);
    if (endptr == comma + 1 ||
            endptr == NULL ||
            endptr[0] != '\0' ||
            errno == ERANGE ||
            parsed_y > (long)INT_MAX ||
            parsed_y < (long)INT_MIN) {
        sixel_helper_set_additional_message(
            "-d bluenoise:phase must be in form X,Y.");
        return SIXEL_BAD_ARGUMENT;
    }

    *phase_x_out = (int)parsed_x;
    *phase_y_out = (int)parsed_y;
    return SIXEL_OK;
}

static int
sixel_encoder_resolve_stbn_default_spatial_diffuse(void)
{
    char const *value;
    int resolved;

    value = sixel_compat_getenv(SIXEL_DITHER_STBN_DIFFUSION_ENVVAR);
    resolved = sixel_interframe_spatial_diffuse_from_string(value);
    if (resolved == SIXEL_INTERFRAME_SPATIAL_DIFFUSE_UNSET) {
        return SIXEL_DIFFUSE_NONE;
    }

    return resolved;
}

static SIXELSTATUS
sixel_encoder_parse_quantize_threshold_text(
    char const *text,
    double *threshold_out)
{
    char *endptr;
    double parsed;

    endptr = NULL;
    parsed = 0.0;
    if (text == NULL || threshold_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtod(text, &endptr);
    if (endptr == text ||
            endptr == NULL ||
            endptr[0] != '\0' ||
            errno != 0 ||
            parsed < 0.0 ||
            parsed > 0.5) {
        sixel_helper_set_additional_message(
            "-Q threshold must be in range 0.0-0.5.");
        return SIXEL_BAD_ARGUMENT;
    }

    *threshold_out = parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_quantize_animation_mode_text(
    char const *text,
    int *mode_out)
{
    char *endptr;
    long parsed;

    endptr = NULL;
    parsed = 0L;
    if (text == NULL || mode_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (endptr == text || endptr == NULL || endptr[0] != '\0'
            || errno == ERANGE || (parsed != 0L && parsed != 1L)) {
        sixel_helper_set_additional_message(
            "-Q animation_mode must be 0 or 1.");
        return SIXEL_BAD_ARGUMENT;
    }

    *mode_out = (int)parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_quantize_scene_cut_threshold_text(
    char const *text,
    double *threshold_out)
{
    char *endptr;
    double parsed;

    endptr = NULL;
    parsed = 0.0;
    if (text == NULL || threshold_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtod(text, &endptr);
    if (endptr == text || endptr == NULL || endptr[0] != '\0'
            || errno != 0 || parsed != parsed
            || parsed < 0.0 || parsed > 1.0) {
        sixel_helper_set_additional_message(
            "-Q scene_cut_threshold must be in range 0.0-1.0.");
        return SIXEL_BAD_ARGUMENT;
    }

    *threshold_out = parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_kmeans_binbits_text(
    char const *text,
    unsigned int *bits_out)
{
    char *endptr;
    long parsed;

    endptr = NULL;
    parsed = 0L;
    if (text == NULL || bits_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (endptr == text ||
            endptr == NULL ||
            endptr[0] != '\0' ||
            errno != 0 ||
            parsed < 4L ||
            parsed > 8L) {
        sixel_helper_set_additional_message(
            "-Q binbits must be in range 4-8.");
        return SIXEL_BAD_ARGUMENT;
    }

    *bits_out = (unsigned int)parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_kmeans_autoratio_text(
    char const *text,
    unsigned int *ratio_out)
{
    char *endptr;
    long parsed;

    endptr = NULL;
    parsed = 0L;
    if (text == NULL || ratio_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (endptr == text ||
            endptr == NULL ||
            endptr[0] != '\0' ||
            errno != 0 ||
            parsed < 1L ||
            parsed > 1048576L) {
        sixel_helper_set_additional_message(
            "-Q autoratio must be in range 1-1048576.");
        return SIXEL_BAD_ARGUMENT;
    }

    *ratio_out = (unsigned int)parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_kmedoids_seed_text(
    char const *text,
    uint32_t *seed_out)
{
    char *endptr;
    unsigned long long parsed;

    endptr = NULL;
    parsed = 0u;
    if (text == NULL || seed_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtoull(text, &endptr, 10);
    if (endptr == text ||
            endptr == NULL ||
            endptr[0] != '\0' ||
            errno != 0 ||
            parsed > 0xffffffffULL) {
        sixel_helper_set_additional_message(
            "-Q seed must be in range 0-4294967295.");
        return SIXEL_BAD_ARGUMENT;
    }

    *seed_out = (uint32_t)parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_kmedoids_uint_text(
    char const *text,
    unsigned int minimum,
    unsigned int maximum,
    int allow_zero,
    char const *label,
    unsigned int *value_out)
{
    char *endptr;
    unsigned long long parsed;
    char message[160];

    endptr = NULL;
    parsed = 0u;
    message[0] = '\0';
    if (text == NULL || label == NULL || value_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtoull(text, &endptr, 10);
    if (endptr == text || endptr == NULL || endptr[0] != '\0'
            || errno != 0 || parsed > (unsigned long long)UINT_MAX) {
        goto invalid;
    }
    if (allow_zero && parsed == 0u) {
        *value_out = 0u;
        return SIXEL_OK;
    }
    if (parsed < (unsigned long long)minimum
            || parsed > (unsigned long long)maximum) {
        goto invalid;
    }

    *value_out = (unsigned int)parsed;
    return SIXEL_OK;

invalid:
    if (allow_zero) {
        (void)sixel_compat_snprintf(
            message,
            sizeof(message),
            "-Q %s must be 0 or in range %u-%u.",
            label,
            minimum,
            maximum);
    } else {
        (void)sixel_compat_snprintf(
            message,
            sizeof(message),
            "-Q %s must be in range %u-%u.",
            label,
            minimum,
            maximum);
    }
    sixel_helper_set_additional_message(message);
    return SIXEL_BAD_ARGUMENT;
}

static SIXELSTATUS
sixel_encoder_parse_kmedoids_prune_mass_text(
    char const *text,
    double *value_out)
{
    char *endptr;
    double parsed;

    endptr = NULL;
    parsed = 0.0;
    if (text == NULL || value_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtod(text, &endptr);
    if (endptr == text || endptr == NULL || endptr[0] != '\0'
            || errno != 0 || parsed != parsed
            || parsed < 0.900 || parsed > 1.000) {
        sixel_helper_set_additional_message(
            "-Q prune_mass must be in range 0.900-1.000.");
        return SIXEL_BAD_ARGUMENT;
    }

    *value_out = parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_kcenter_budget_scale_text(
    char const *text,
    double *value_out)
{
    char *endptr;
    double parsed;

    endptr = NULL;
    parsed = 0.0;
    if (text == NULL || value_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtod(text, &endptr);
    if (endptr == text || endptr == NULL || endptr[0] != '\0'
            || errno != 0 || parsed != parsed
            || parsed < 0.25 || parsed > 4.00) {
        sixel_helper_set_additional_message(
            "-Q budget_scale must be in range 0.25-4.00.");
        return SIXEL_BAD_ARGUMENT;
    }

    *value_out = parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_kcenter_swap_min_gain_text(
    char const *text,
    double *value_out)
{
    char *endptr;
    double parsed;

    endptr = NULL;
    parsed = 0.0;
    if (text == NULL || value_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtod(text, &endptr);
    if (endptr == text || endptr == NULL || endptr[0] != '\0'
            || errno != 0 || parsed != parsed
            || parsed < 0.0 || parsed > 8.0) {
        sixel_helper_set_additional_message(
            "-Q swap_min_gain must be in range 0.0-8.0.");
        return SIXEL_BAD_ARGUMENT;
    }

    *value_out = parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_quantize_merge_oversplit_text(
    char const *text,
    double *value_out)
{
    char *endptr;
    double parsed;

    endptr = NULL;
    parsed = 0.0;
    if (text == NULL || value_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    errno = 0;
    parsed = strtod(text, &endptr);
    if (endptr == text || endptr == NULL || endptr[0] != '\0'
            || errno != 0 || parsed != parsed
            || parsed < 1.0 || parsed > 3.0) {
        sixel_helper_set_additional_message(
            "-Q merge_oversplit must be in range 1.0-3.0.");
        return SIXEL_BAD_ARGUMENT;
    }

    *value_out = parsed;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_validate_quantize_model_resolution(
    sixel_option_argument_resolution_t const *resolution)
{
    size_t index;
    sixel_suboption_assignment_t const *assignment;
    char const *key_name;
    SIXELSTATUS status;
    int base_value;
    int resolved_choice;
    double parsed_double;
    unsigned int parsed_uint;
    uint32_t seed;

    index = 0u;
    assignment = NULL;
    key_name = NULL;
    status = SIXEL_OK;
    base_value = 0;
    resolved_choice = 0;
    parsed_double = 0.0;
    parsed_uint = 0u;
    seed = 0u;
    if (resolution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    base_value = resolution->resolved_base_value;

    while (index < resolution->assignment_count) {
        assignment = resolution->assignments + index;
        key_name = assignment->resolved_key_name;
        if (key_name == NULL) {
            ++index;
            continue;
        }
        if (key_name != NULL && strcmp(key_name, "animation_mode") == 0) {
            status = sixel_encoder_parse_quantize_animation_mode_text(
                assignment->resolved_value_text,
                &resolved_choice);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "scene_cut_threshold") == 0) {
            status = sixel_encoder_parse_quantize_scene_cut_threshold_text(
                assignment->resolved_value_text,
                &parsed_double);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else
        if (key_name != NULL && strcmp(key_name, "threshold") == 0) {
            status = sixel_encoder_parse_quantize_threshold_text(
                assignment->resolved_value_text,
                &parsed_double);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "binbits") == 0) {
            status = sixel_encoder_parse_kmeans_binbits_text(
                assignment->resolved_value_text,
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "autoratio") == 0) {
            status = sixel_encoder_parse_kmeans_autoratio_text(
                assignment->resolved_value_text,
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "seed") == 0) {
            status = sixel_encoder_parse_kmedoids_seed_text(
                assignment->resolved_value_text,
                &seed);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "algo") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                sixel_helper_set_additional_message(
                    "invalid -Q algo resolution.");
                return SIXEL_BAD_ARGUMENT;
            }
        } else if (key_name != NULL && strcmp(key_name, "profile") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                sixel_helper_set_additional_message(
                    "invalid -Q profile resolution.");
                return SIXEL_BAD_ARGUMENT;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "auto_policy") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                sixel_helper_set_additional_message(
                    "invalid -Q auto_policy resolution.");
                return SIXEL_BAD_ARGUMENT;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "space_policy") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                sixel_helper_set_additional_message(
                    "invalid -Q space_policy resolution.");
                return SIXEL_BAD_ARGUMENT;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "candidate_policy") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                sixel_helper_set_additional_message(
                    "invalid -Q candidate_policy resolution.");
                return SIXEL_BAD_ARGUMENT;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "budget_policy") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                sixel_helper_set_additional_message(
                    "invalid -Q budget_policy resolution.");
                return SIXEL_BAD_ARGUMENT;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "swap_update") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                sixel_helper_set_additional_message(
                    "invalid -Q swap_update resolution.");
                return SIXEL_BAD_ARGUMENT;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "auto_fft_threshold") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                256u,
                65536u,
                0,
                "auto_fft_threshold",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "iter") == 0) {
            if (base_value == SIXEL_QUANTIZE_MODEL_KMEANS) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    assignment->resolved_value_text,
                    1u,
                    100u,
                    0,
                    "iter",
                    &parsed_uint);
            } else {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    assignment->resolved_value_text,
                    1u,
                    64u,
                    0,
                    "iter",
                    &parsed_uint);
            }
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "iter_max") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                100u,
                0,
                "iter_max",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "miniter") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                100u,
                1,
                "miniter",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "polish_iter") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                16u,
                1,
                "polish_iter",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "restarts") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                32u,
                0,
                "restarts",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "init_seeds") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                8u,
                0,
                "init_seeds",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "feedback_slots") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                16u,
                0,
                "feedback_slots",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "feedback_interval") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                64u,
                0,
                "feedback_interval",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "sample") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                64u,
                1048576u,
                1,
                "sample",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "clara_trials") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                32u,
                0,
                "clara_trials",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "clara_sample") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                64u,
                1048576u,
                1,
                "clara_sample",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "clarans_local") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                32u,
                0,
                "clarans_local",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "clarans_neighbors") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                5000000u,
                1,
                "clarans_neighbors",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "bandit_iter") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                64u,
                0,
                "bandit_iter",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "bandit_candidates") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                8u,
                4096u,
                0,
                "bandit_candidates",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "bandit_batch") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                8u,
                4096u,
                0,
                "bandit_batch",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "histbits") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                3u,
                6u,
                0,
                "histbits",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "point_budget") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                64u,
                16384u,
                base_value == SIXEL_QUANTIZE_MODEL_KCENTER,
                "point_budget",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "rare_keep") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                base_value == SIXEL_QUANTIZE_MODEL_KCENTER ? 1u : 0u,
                base_value == SIXEL_QUANTIZE_MODEL_KCENTER ? 2048u : 1024u,
                base_value == SIXEL_QUANTIZE_MODEL_KCENTER,
                "rare_keep",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "prune_mass") == 0) {
            status = sixel_encoder_parse_kmedoids_prune_mass_text(
                assignment->resolved_value_text,
                &parsed_double);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "budget_scale") == 0) {
            status = sixel_encoder_parse_kcenter_budget_scale_text(
                assignment->resolved_value_text,
                &parsed_double);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "swap_topk") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                16u,
                0,
                "swap_topk",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "swap_patience") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                8u,
                1,
                "swap_patience",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "swap_min_gain") == 0) {
            status = sixel_encoder_parse_kcenter_swap_min_gain_text(
                assignment->resolved_value_text,
                &parsed_double);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "auction") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                sixel_helper_set_additional_message(
                    "invalid -Q auction resolution.");
                return SIXEL_BAD_ARGUMENT;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "auction_shortlist") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                2u,
                8u,
                0,
                "auction_shortlist",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL
                && strcmp(key_name, "merge_oversplit") == 0) {
            status = sixel_encoder_parse_quantize_merge_oversplit_text(
                assignment->resolved_value_text,
                &parsed_double);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "merge_lloyd") == 0) {
            status = sixel_encoder_parse_kmedoids_uint_text(
                assignment->resolved_value_text,
                1u,
                30u,
                1,
                "merge_lloyd",
                &parsed_uint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else if (key_name != NULL && strcmp(key_name, "merge") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                sixel_helper_set_additional_message(
                    "invalid -Q merge resolution.");
                return SIXEL_BAD_ARGUMENT;
            }
        }
        ++index;
    }

    return SIXEL_OK;
}

static int
sixel_encoder_resolve_suboption_choice_value(
    sixel_suboption_assignment_t const *assignment,
    int *value_out)
{
    size_t index;
    sixel_suboption_key_t const *key_def;
    sixel_suboption_choice_t const *choice;

    index = 0u;
    key_def = NULL;
    choice = NULL;
    if (assignment == NULL || value_out == NULL ||
            assignment->key_def == NULL ||
            assignment->resolved_value_text == NULL) {
        return 0;
    }

    key_def = assignment->key_def;
    if (key_def->value_kind != SIXEL_SUBOPTION_VALUE_CHOICE ||
            key_def->choices == NULL ||
            key_def->choice_count == 0u) {
        return 0;
    }

    while (index < key_def->choice_count) {
        choice = key_def->choices + index;
        if (choice->name != NULL &&
                strcmp(choice->name, assignment->resolved_value_text) == 0) {
            *value_out = choice->value;
            return 1;
        }
        ++index;
    }

    return 0;
}

static SIXELSTATUS
sixel_encoder_parse_quantize_model_argument(
    char const *value,
    sixel_option_argument_list_resolution_t *resolution,
    char *diagnostic,
    size_t diagnostic_size)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (resolution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_option_parse_argument_list_with_suboptions(
        value,
        &g_schema_quantize_model,
        resolution,
        diagnostic,
        diagnostic_size);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (resolution->item_count != 1u || resolution->has_trailing_bang) {
        sixel_helper_set_additional_message(
            "-Q expects exactly one MODEL[:KEY=VALUE] item.");
        sixel_option_free_argument_list_resolution(resolution);
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_encoder_validate_quantize_model_resolution(
        &resolution->items[0].resolution);
    if (SIXEL_FAILED(status)) {
        sixel_option_free_argument_list_resolution(resolution);
    }

    return status;
}

static SIXELSTATUS
sixel_encoder_parse_diffusion_argument(
    char const *value,
    sixel_option_argument_resolution_t *resolution,
    char *diagnostic,
    size_t diagnostic_size)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (resolution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_option_parse_argument_with_suboptions(
        value,
        &g_schema_diffusion,
        resolution,
        diagnostic,
        diagnostic_size);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return SIXEL_OK;
}

static int
sixel_encoder_lut_policy_supports_shared_instance(int lut_policy)
{
    if (lut_policy == SIXEL_LUT_POLICY_5BIT
            || lut_policy == SIXEL_LUT_POLICY_6BIT
            || lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        return 1;
    }

    return 0;
}

static SIXELSTATUS
sixel_encoder_parse_lut_policy_argument(
    char const *value,
    int *lut_policy_out,
    int *shared_override_out,
    int *shared_value_out,
    char *diagnostic,
    size_t diagnostic_size)
{
    SIXELSTATUS status;
    sixel_option_argument_resolution_t resolution;
    sixel_suboption_assignment_t const *assignment;
    size_t index;
    int match_value;
    int shared_override;
    int shared_value;

    status = SIXEL_OK;
    resolution.resolved_base_value = 0;
    resolution.base_def = NULL;
    resolution.assignments = NULL;
    resolution.assignment_count = 0u;
    assignment = NULL;
    index = 0u;
    match_value = SIXEL_LUT_POLICY_AUTO;
    shared_override = 0;
    shared_value = 0;

    if (lut_policy_out == NULL || shared_override_out == NULL
            || shared_value_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_option_parse_argument_with_suboptions(
        value,
        &g_schema_lookup_policy,
        &resolution,
        diagnostic,
        diagnostic_size);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    match_value = resolution.resolved_base_value;
    if (resolution.assignment_count > 0u
            && !sixel_encoder_lut_policy_supports_shared_instance(
                match_value)) {
        sixel_helper_set_additional_message(
            "-~ shared_instance is supported only for 5bit, 6bit, and "
            "certlut.");
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    while (index < resolution.assignment_count) {
        assignment = resolution.assignments + index;
        if (assignment->resolved_key_name != NULL
                && strcmp(assignment->resolved_key_name,
                          "shared_instance") != 0) {
            sixel_helper_set_additional_message(
                "-~ accepts only shared_instance suboption.");
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }
        if (assignment->resolved_key_name != NULL) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &shared_value)) {
                sixel_helper_set_additional_message(
                    "invalid -~ shared_instance resolution.");
                status = SIXEL_BAD_ARGUMENT;
                goto cleanup;
            }
            shared_override = 1;
        }
        ++index;
    }

    *lut_policy_out = match_value;
    *shared_override_out = shared_override;
    *shared_value_out = (shared_value != 0) ? 1 : 0;
    status = SIXEL_OK;

cleanup:
    sixel_option_free_argument_resolution(&resolution);
    return status;
}

static SIXELSTATUS
sixel_encoder_resolve_sierra_suboptions(
    sixel_option_argument_resolution_t const *resolution,
    int *resolved_diffusion)
{
    size_t index;
    sixel_suboption_assignment_t const *assignment;
    int resolved_variant;

    index = 0u;
    assignment = NULL;
    resolved_variant = SIXEL_DIFFUSE_SIERRA1;
    if (resolution == NULL || resolved_diffusion == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *resolved_diffusion = resolution->resolved_base_value;
    if (resolution->base_def == NULL
            || resolution->base_def->name == NULL
            || strcmp(resolution->base_def->name, "sierra") != 0) {
        return SIXEL_OK;
    }

    *resolved_diffusion = SIXEL_DIFFUSE_SIERRA1;
    while (index < resolution->assignment_count) {
        assignment = resolution->assignments + index;
        if (assignment->resolved_key_name != NULL
                && strcmp(assignment->resolved_key_name, "variant") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_variant)) {
                return SIXEL_BAD_ARGUMENT;
            }
            *resolved_diffusion = resolved_variant;
        }
        ++index;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_resolve_diffusion_scan_suboptions(
    sixel_option_argument_resolution_t const *resolution,
    int *scan_mode)
{
    size_t index;
    sixel_suboption_assignment_t const *assignment;
    int resolved_scan;

    index = 0u;
    assignment = NULL;
    resolved_scan = SIXEL_SCAN_AUTO;
    if (resolution == NULL || scan_mode == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *scan_mode = SIXEL_SCAN_AUTO;
    while (index < resolution->assignment_count) {
        assignment = resolution->assignments + index;
        if (assignment->resolved_key_name != NULL
                && strcmp(assignment->resolved_key_name, "scan") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_scan)) {
                return SIXEL_BAD_ARGUMENT;
            }
            *scan_mode = resolved_scan;
        }
        ++index;
    }

    return SIXEL_OK;
}

/*
 * pcc may ICE on very long parameter lists.  Bundle interframe/bluenoise
 * suboption outputs into compact state objects.
 */
typedef struct sixel_encoder_interframe_suboption_state {
    int has_strategy_override;
    int strategy_token;
    int has_spatial_diffuse_override;
    int spatial_diffuse;
    int has_noise_strength_override;
    int noise_strength_u8;
    int has_motion_adapt_override;
    int motion_adapt_enabled;
    int has_scene_cut_reset_override;
    int scene_cut_reset_enabled;
    int has_scene_detect_override;
    int scene_detect_enabled;
    int has_alpha_guard_override;
    int alpha_guard_enabled;
    int has_perceptual_weight_override;
    int perceptual_weight_enabled;
    int has_fastpath_override;
    int fastpath_enabled;
} sixel_encoder_interframe_suboption_state_t;

typedef struct sixel_encoder_bluenoise_suboption_state {
    int has_strength_override;
    float strength;
    int has_gradient_factor_override;
    float gradient_factor;
    int has_phase_override;
    int phase_x;
    int phase_y;
    int has_seed_override;
    int seed;
    int has_channel_override;
    int channel_rgb;
    int has_size_override;
    int size;
} sixel_encoder_bluenoise_suboption_state_t;

static SIXELSTATUS
sixel_encoder_resolve_interframe_suboptions(
    sixel_option_argument_resolution_t const *resolution,
    sixel_encoder_interframe_suboption_state_t *state)
{
    SIXELSTATUS status;
    size_t index;
    sixel_suboption_assignment_t const *assignment;
    char const *base_name;
    char const *resolved_key_name;
    int resolved_choice;
    int resolved_spatial_diffuse;
    int resolved_strength_u8;

    status = SIXEL_OK;
    index = 0u;
    assignment = NULL;
    base_name = NULL;
    resolved_key_name = NULL;
    resolved_choice = SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE;
    resolved_spatial_diffuse = SIXEL_DIFFUSE_FS;
    resolved_strength_u8 = 0;
    if (resolution == NULL || state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state->has_strategy_override = 0;
    state->strategy_token = SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE;
    state->has_spatial_diffuse_override = 0;
    state->spatial_diffuse = SIXEL_DIFFUSE_FS;
    state->has_noise_strength_override = 0;
    state->noise_strength_u8 = 0;
    state->has_motion_adapt_override = 0;
    state->motion_adapt_enabled = 0;
    state->has_scene_cut_reset_override = 0;
    state->scene_cut_reset_enabled = 0;
    state->has_scene_detect_override = 0;
    state->scene_detect_enabled = 0;
    state->has_alpha_guard_override = 0;
    state->alpha_guard_enabled = 0;
    state->has_perceptual_weight_override = 0;
    state->perceptual_weight_enabled = 0;
    state->has_fastpath_override = 0;
    state->fastpath_enabled = 0;
    if (resolution->resolved_base_value != SIXEL_DIFFUSE_INTERFRAME) {
        return SIXEL_OK;
    }

    if (resolution->base_def != NULL) {
        base_name = resolution->base_def->name;
    }
    if (base_name != NULL && strcmp(base_name, "interframe") == 0) {
        state->has_strategy_override = 1;
        state->strategy_token = SIXEL_INTERFRAME_STRATEGY_TOKEN_DIFFUSION;
    } else if (base_name != NULL && strcmp(base_name, "stbn") == 0) {
        state->has_strategy_override = 1;
        state->strategy_token = SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_HASH;
        /*
         * Keep stbn defaults explicit: absent diffusion suboption means
         * diffusion=none unless the shared interframe diffusion envvar
         * explicitly requests another kernel.
         */
        state->has_spatial_diffuse_override = 1;
        state->spatial_diffuse =
            sixel_encoder_resolve_stbn_default_spatial_diffuse();
        resolved_choice = sixel_interframe_strategy_token_from_env_common();
        if (sixel_interframe_strategy_method_from_token(resolved_choice)
                == SIXEL_INTERFRAME_METHOD_STBN) {
            state->strategy_token = resolved_choice;
        }
    } else {
        return SIXEL_OK;
    }

    while (index < resolution->assignment_count) {
        assignment = resolution->assignments + index;
        resolved_key_name = assignment->resolved_key_name;
        if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "source") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                return SIXEL_BAD_ARGUMENT;
            }
            state->has_strategy_override = 1;
            state->strategy_token = resolved_choice;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "diffusion") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_spatial_diffuse)) {
                return SIXEL_BAD_ARGUMENT;
            }
            state->has_spatial_diffuse_override = 1;
            state->spatial_diffuse = resolved_spatial_diffuse;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "strength") == 0) {
            status = sixel_encoder_parse_interframe_noise_strength_text(
                assignment->resolved_value_text,
                &resolved_strength_u8);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            state->has_noise_strength_override = 1;
            state->noise_strength_u8 = resolved_strength_u8;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "motion_adapt") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                return SIXEL_BAD_ARGUMENT;
            }
            state->has_motion_adapt_override = 1;
            state->motion_adapt_enabled = resolved_choice;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "scene_cut_reset") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                return SIXEL_BAD_ARGUMENT;
            }
            state->has_scene_cut_reset_override = 1;
            state->scene_cut_reset_enabled = resolved_choice;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "scene_detect") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                return SIXEL_BAD_ARGUMENT;
            }
            state->has_scene_detect_override = 1;
            state->scene_detect_enabled = resolved_choice;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "alpha_guard") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                return SIXEL_BAD_ARGUMENT;
            }
            state->has_alpha_guard_override = 1;
            state->alpha_guard_enabled = resolved_choice;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "perceptual_weight")
                == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                return SIXEL_BAD_ARGUMENT;
            }
            state->has_perceptual_weight_override = 1;
            state->perceptual_weight_enabled = resolved_choice;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "fastpath") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                return SIXEL_BAD_ARGUMENT;
            }
            state->has_fastpath_override = 1;
            state->fastpath_enabled = resolved_choice;
        }
        ++index;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_resolve_bluenoise_suboptions(
    sixel_option_argument_resolution_t const *resolution,
    sixel_encoder_bluenoise_suboption_state_t *state)
{
    SIXELSTATUS status;
    size_t index;
    sixel_suboption_assignment_t const *assignment;
    char const *resolved_key_name;
    int resolved_choice;
    float resolved_strength;
    float resolved_gradient_factor;
    int resolved_phase_x;
    int resolved_phase_y;
    int resolved_seed;

    status = SIXEL_OK;
    index = 0u;
    assignment = NULL;
    resolved_key_name = NULL;
    resolved_choice = 0;
    resolved_strength = 0.055f;
    resolved_gradient_factor = 0.0f;
    resolved_phase_x = 0;
    resolved_phase_y = 0;
    resolved_seed = 0;
    if (resolution == NULL || state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state->has_strength_override = 0;
    state->strength = resolved_strength;
    state->has_gradient_factor_override = 0;
    state->gradient_factor = 0.0f;
    state->has_phase_override = 0;
    state->phase_x = 0;
    state->phase_y = 0;
    state->has_seed_override = 0;
    state->seed = 0;
    state->has_channel_override = 0;
    state->channel_rgb = 0;
    state->has_size_override = 0;
    state->size = 64;
    if (resolution->resolved_base_value != SIXEL_DIFFUSE_BLUENOISE_DITHER) {
        return SIXEL_OK;
    }

    while (index < resolution->assignment_count) {
        assignment = resolution->assignments + index;
        resolved_key_name = assignment->resolved_key_name;
        if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "strength") == 0) {
            status = sixel_encoder_parse_bluenoise_strength_text(
                assignment->resolved_value_text,
                &resolved_strength);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            state->has_strength_override = 1;
            state->strength = resolved_strength;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "gradient_factor") == 0) {
            status = sixel_encoder_parse_bluenoise_gradient_factor_text(
                assignment->resolved_value_text,
                &resolved_gradient_factor);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            state->has_gradient_factor_override = 1;
            state->gradient_factor = resolved_gradient_factor;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "phase") == 0) {
            status = sixel_encoder_parse_bluenoise_phase_text(
                assignment->resolved_value_text,
                &resolved_phase_x,
                &resolved_phase_y);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            state->has_phase_override = 1;
            state->phase_x = resolved_phase_x;
            state->phase_y = resolved_phase_y;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "seed") == 0) {
            status = sixel_encoder_parse_bluenoise_seed_text(
                assignment->resolved_value_text,
                &resolved_seed);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            state->has_seed_override = 1;
            state->seed = resolved_seed;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "channel") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                return SIXEL_BAD_ARGUMENT;
            }
            state->has_channel_override = 1;
            state->channel_rgb = resolved_choice;
        } else if (resolved_key_name != NULL
                && strcmp(resolved_key_name, "size") == 0) {
            if (!sixel_encoder_resolve_suboption_choice_value(
                    assignment,
                    &resolved_choice)) {
                return SIXEL_BAD_ARGUMENT;
            }
            state->has_size_override = 1;
            state->size = resolved_choice;
        }
        ++index;
    }

    return SIXEL_OK;
}

typedef struct sixel_encoder_setopt_context {
    sixel_option_argument_list_resolution_t q_list_resolution;
} sixel_encoder_setopt_context_t;

static void
sixel_encoder_setopt_context_init(sixel_encoder_setopt_context_t *context)
{
    if (context == NULL) {
        return;
    }

    sixel_option_init_argument_list_resolution(&context->q_list_resolution);
}

static void
sixel_encoder_setopt_context_dispose(
    sixel_encoder_setopt_context_t *context)
{
    if (context == NULL) {
        return;
    }

    sixel_option_free_argument_list_resolution(&context->q_list_resolution);
}

static SIXELSTATUS
sixel_encoder_parse_choice_argument(
    char const *value,
    sixel_option_choice_t const *choices,
    size_t choice_count,
    char const *invalid_message,
    int *match_value_out)
{
    sixel_option_choice_result_t match_result;
    int match_value;
    char match_detail[128];
    char match_message[256];

    match_result = SIXEL_OPTION_CHOICE_NONE;
    match_value = 0;
    if (choices == NULL || choice_count == 0u ||
        invalid_message == NULL || match_value_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    match_result = sixel_option_match_choice(value,
                                             choices,
                                             choice_count,
                                             &match_value,
                                             match_detail,
                                             sizeof(match_detail));
    if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
        *match_value_out = match_value;
        return SIXEL_OK;
    }

    if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
        sixel_option_report_ambiguous_prefix(value,
                                             match_detail,
                                             match_message,
                                             sizeof(match_message));
    } else {
        sixel_option_report_invalid_choice(invalid_message,
                                           match_detail,
                                           match_message,
                                           sizeof(match_message));
    }

    return SIXEL_BAD_ARGUMENT;
}

static SIXELSTATUS
sixel_encoder_apply_threads_option(char const *value)
{
    int number;

    if (sixel_encoder_parse_threads_argument(value, &number) == 0) {
        sixel_helper_set_additional_message(
            "threads accepts positive integers or 'auto'.");
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_set_threads(number);
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_encoder_apply_colors_option(
    sixel_encoder_t *encoder,
    char const *value)
{
    char *endptr;
    long parsed_reqcolors;
    int forced_palette;

    endptr = NULL;
    parsed_reqcolors = 0L;
    forced_palette = 0;
    errno = 0;
    if (*value == '!' && value[1] == '\0') {
        /*
         * Force the default palette size even when the median cut
         * finished early.
         *
         *   requested colors
         *          |
         *          v
         *        [ 256 ]  <--- "-p!" triggers this shortcut
         */
        parsed_reqcolors = SIXEL_PALETTE_MAX;
        forced_palette = 1;
    } else {
        if (value[0] == '-') {
            /*
             * Reject negative palettes explicitly instead of relying on
             * platform-specific strtol() details.
             */
            sixel_helper_set_additional_message(
                "-p/--colors parameter must be 1 or more.");
            return SIXEL_BAD_ARGUMENT;
        }
        parsed_reqcolors = strtol(value, &endptr, 10);
        if (endptr != NULL && *endptr == '!') {
            forced_palette = 1;
            ++endptr;
        }
        if (errno == ERANGE || endptr == value) {
            sixel_helper_set_additional_message(
                "cannot parse -p/--colors option.");
            return SIXEL_BAD_ARGUMENT;
        }
        if (endptr != NULL && *endptr != '\0') {
            sixel_helper_set_additional_message(
                "cannot parse -p/--colors option.");
            return SIXEL_BAD_ARGUMENT;
        }
    }

    if (parsed_reqcolors < 1) {
        sixel_helper_set_additional_message(
            "-p/--colors parameter must be 1 or more.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (parsed_reqcolors > SIXEL_PALETTE_MAX) {
        sixel_helper_set_additional_message(
            "-p/--colors parameter must be less then or equal to 256.");
        return SIXEL_BAD_ARGUMENT;
    }

    encoder->reqcolors = (int)parsed_reqcolors;
    encoder->force_palette = forced_palette;
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_encoder_apply_crop_option(
    sixel_encoder_t *encoder,
    char const *value)
{
    int geometry_ok;

    geometry_ok = sixel_encoder_parse_crop_geometry(value,
                                                    &encoder->clipwidth,
                                                    &encoder->clipheight,
                                                    &encoder->clipx,
                                                    &encoder->clipy);
    if (geometry_ok == 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    encoder->clipfirst = 0;
    return SIXEL_OK;
}


static void
sixel_encoder_set_cell_probe_error(
    char const *base_message,
    char const *prefix_message)
{
    size_t prefix_length;
    size_t detail_length;
    char message[256];
    char const *detail;

    detail = sixel_helper_get_additional_message();
    if (detail == NULL || detail[0] == '\0') {
        sixel_helper_set_additional_message(base_message);
        return;
    }

    prefix_length = strlen(prefix_message);
    detail_length = strnlen(detail, sizeof(message));
    if (prefix_length + detail_length >= sizeof(message)) {
        detail_length = sizeof(message) - prefix_length - 1U;
    }
    (void)snprintf(message,
                   sizeof(message),
                   "%s%.*s",
                   prefix_message,
                   (int)detail_length,
                   detail);
    sixel_helper_set_additional_message(message);
}


static SIXELSTATUS
sixel_encoder_apply_dimension_option(
    sixel_encoder_t *encoder,
    char const *value,
    int *pixel_target,
    int *percent_target,
    int clip_value,
    int cell_scale,
    char const *parse_error,
    char const *percent_error,
    char const *cells_error,
    char const *pixel_error,
    char const *cell_probe_error,
    char const *cell_probe_prefix)
{
    SIXELSTATUS status;
    int number;
    long parsed_value;
    char const *suffix;

    status = SIXEL_OK;
    number = 0;
    parsed_value = 0L;
    suffix = NULL;
    if (strcmp(value, "auto") == 0) {
        *pixel_target = (-1);
        *percent_target = (-1);
        return SIXEL_OK;
    }

    if (!sixel_encoder_parse_dimension_value(value, &parsed_value, &suffix)) {
        sixel_helper_set_additional_message(parse_error);
        return SIXEL_BAD_ARGUMENT;
    }
    if (parsed_value > (long)INT_MAX) {
        sixel_helper_set_additional_message(parse_error);
        return SIXEL_BAD_ARGUMENT;
    }

    number = (int)parsed_value;
    if (suffix[0] == '%' && suffix[1] == '\0') {
        if (number <= 0) {
            sixel_helper_set_additional_message(percent_error);
            return SIXEL_BAD_ARGUMENT;
        }
        *pixel_target = (-1);
        *percent_target = number;
    } else if (suffix[0] == 'c' && suffix[1] == '\0') {
        status = sixel_encoder_ensure_cell_size(encoder);
        if (SIXEL_FAILED(status)) {
            sixel_encoder_set_cell_probe_error(cell_probe_error,
                                               cell_probe_prefix);
            return status;
        }
        if (number <= 0) {
            sixel_helper_set_additional_message(cells_error);
            return SIXEL_BAD_ARGUMENT;
        }
        *pixel_target = number * cell_scale;
        *percent_target = (-1);
    } else if (suffix[0] == '\0'
               || (suffix[0] == 'p'
                   && suffix[1] == 'x'
                   && suffix[2] == '\0')) {
        if (number <= 0) {
            sixel_helper_set_additional_message(pixel_error);
            return SIXEL_BAD_ARGUMENT;
        }
        *pixel_target = number;
        *percent_target = (-1);
    } else {
        sixel_helper_set_additional_message(parse_error);
        return SIXEL_BAD_ARGUMENT;
    }

    if (clip_value != 0) {
        encoder->clipfirst = 1;
    }
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_encoder_apply_width_option(
    sixel_encoder_t *encoder,
    char const *value)
{
    return sixel_encoder_apply_dimension_option(
        encoder,
        value,
        &encoder->pixelwidth,
        &encoder->percentwidth,
        encoder->clipwidth,
        encoder->cell_width,
        "cannot parse -w/--width option.",
        "-w/--width percent must be 1 or more.",
        "-w/--width cells must be 1 or more.",
        "-w/--width must be 1 or more.",
        "cannot determine terminal cell size for -w/--width option.",
        "cannot determine terminal cell size for -w/--width option: ");
}


static SIXELSTATUS
sixel_encoder_apply_height_option(
    sixel_encoder_t *encoder,
    char const *value)
{
    return sixel_encoder_apply_dimension_option(
        encoder,
        value,
        &encoder->pixelheight,
        &encoder->percentheight,
        encoder->clipheight,
        encoder->cell_height,
        "cannot parse -h/--height option.",
        "-h/--height percent must be 1 or more.",
        "-h/--height cells must be 1 or more.",
        "-h/--height must be 1 or more.",
        "cannot determine terminal cell size for -h/--height option.",
        "cannot determine terminal cell size for -h/--height option: ");
}


static SIXELSTATUS
sixel_encoder_apply_start_frame_option(
    sixel_encoder_t *encoder,
    char const *value)
{
    char *endptr;
    long parsed_value;

    endptr = NULL;
    parsed_value = 0L;
    errno = 0;
    parsed_value = strtol(value, &endptr, 10);
    if (endptr == value || *endptr != '\0' || errno == ERANGE ||
        parsed_value < (long)INT_MIN ||
        parsed_value > (long)INT_MAX) {
        sixel_helper_set_additional_message(
            "cannot parse start_frame option.");
        return SIXEL_BAD_ARGUMENT;
    }
    encoder->loader_start_frame_no = (int)parsed_value;
    encoder->loader_start_frame_no_set = 1;
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_encoder_apply_macro_number_option(
    sixel_encoder_t *encoder,
    char const *value)
{
    char *endptr;
    long parsed_value;

    endptr = NULL;
    parsed_value = 0L;
    errno = 0;
    parsed_value = strtol(value, &endptr, 10);
    if (endptr == value || *endptr != '\0' || errno == ERANGE ||
        parsed_value < 0L || parsed_value > (long)INT_MAX) {
        return SIXEL_BAD_ARGUMENT;
    }
    encoder->macro_number = (int)parsed_value;
    return SIXEL_OK;
}


static char const *
sixel_encoder_get_palette_option_name(
    int color_option)
{
    switch (color_option) {
    case SIXEL_COLOR_OPTION_MAPFILE:
        return "-m, --mapfile";
    case SIXEL_COLOR_OPTION_MONOCHROME:
        return "-e, --monochrome";
    case SIXEL_COLOR_OPTION_HIGHCOLOR:
        return "-I, --high-color";
    case SIXEL_COLOR_OPTION_BUILTIN:
        return "-b, --builtin-palette";
    default:
        break;
    }

    return NULL;
}


static SIXELSTATUS
sixel_encoder_check_palette_option_conflict(
    sixel_encoder_t *encoder,
    int new_color_option,
    char const *new_option_name)
{
    char message[128];
    char const *current_option_name;

    message[0] = '\0';
    current_option_name = NULL;
    if (encoder->color_option == SIXEL_COLOR_OPTION_DEFAULT ||
        encoder->color_option == new_color_option) {
        return SIXEL_OK;
    }

    /*
     * Fixed palette and high-color options select mutually exclusive encoder
     * output modes.  Reject the second selector immediately so option order
     * never reinterprets colors by silently replacing the first selector.
     */
    current_option_name = sixel_encoder_get_palette_option_name(
        encoder->color_option);
    if (current_option_name == NULL) {
        return SIXEL_OK;
    }

    (void)sixel_compat_snprintf(message,
                                sizeof(message),
                                "option %s conflicts with %s.",
                                new_option_name,
                                current_option_name);
    sixel_helper_set_additional_message(message);
    return SIXEL_BAD_ARGUMENT;
}


static SIXELSTATUS
sixel_encoder_apply_mapfile_option(
    sixel_encoder_t *encoder,
    char const *value)
{
    SIXELSTATUS status;
    char const *mapfile_view;
    char *mapfile_copy;
    char *mapfile_copy_view;
    char *mapfile_normalized;
    char *libc_buffer;
    char const *libc_path;
    size_t mapfile_offset;
    size_t mapfile_length;
    size_t mapfile_full_length;
    size_t libc_buffer_size;
    unsigned int path_flags;
    int path_check;
    char mapfile_message[256];

    status = SIXEL_OK;
    mapfile_view = NULL;
    mapfile_copy = NULL;
    mapfile_copy_view = NULL;
    mapfile_normalized = NULL;
    libc_buffer = NULL;
    libc_path = NULL;
    mapfile_offset = 0u;
    mapfile_length = 0u;
    mapfile_full_length = 0u;
    libc_buffer_size = 0u;
    path_flags = 0u;
    path_check = 0;
    mapfile_message[0] = '\0';
    if (value == NULL || *value == '\0') {
        sixel_helper_set_additional_message(
            "sixel_encoder_setopt: no mapfile specified.");
        return SIXEL_BAD_ARGUMENT;
    }

    mapfile_view = sixel_palette_strip_prefix(value, NULL);
    if (mapfile_view == NULL) {
        mapfile_view = value;
    }
    /*
     * A recognized TYPE: prefix without PATH still names the exact token the
     * user provided.  Reject it here so platform-specific path probes cannot
     * let the late palette import collapse it into a generic empty path.
     */
    if (mapfile_view != value && *mapfile_view == '\0') {
        sixel_compat_snprintf(mapfile_message,
                              sizeof(mapfile_message),
                              "path \"%s\" not found.",
                              value);
        sixel_helper_set_additional_message(mapfile_message);
        return SIXEL_BAD_ARGUMENT;
    }
    mapfile_length = strlen(value);
    mapfile_offset = (size_t)(mapfile_view - value);
    mapfile_copy = arg_strdup(value, encoder->allocator);
    if (mapfile_copy == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_setopt: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    if (mapfile_offset < mapfile_length) {
        mapfile_copy_view = mapfile_copy + mapfile_offset;
    } else {
        mapfile_copy_view = mapfile_copy;
    }

    /*
     * Normalize only the filesystem payload part. Prefixes such as
     * "rgb:" remain in place so diagnostics still reflect user input.
     */
    libc_buffer_size = sixel_path_to_libc_buffer_size(mapfile_copy_view);
    if (libc_buffer_size > 0u) {
        libc_buffer = (char *)sixel_allocator_malloc(encoder->allocator,
                                                     libc_buffer_size);
        if (libc_buffer == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: sixel_allocator_malloc() failed "
                "for mapfile path buffer.");
            sixel_allocator_free(encoder->allocator, mapfile_copy);
            return SIXEL_BAD_ALLOCATION;
        }

        libc_path = sixel_path_to_libc(mapfile_copy_view,
                                       libc_buffer,
                                       libc_buffer_size);
        if (libc_path == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: invalid mapfile path.");
            sixel_allocator_free(encoder->allocator, libc_buffer);
            sixel_allocator_free(encoder->allocator, mapfile_copy);
            return SIXEL_BAD_ARGUMENT;
        }
        if (libc_buffer_size > SIZE_MAX - mapfile_offset) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: mapfile path is too long.");
            sixel_allocator_free(encoder->allocator, libc_buffer);
            sixel_allocator_free(encoder->allocator, mapfile_copy);
            return SIXEL_BAD_ALLOCATION;
        }
        mapfile_full_length = mapfile_offset + libc_buffer_size;
        mapfile_normalized = (char *)sixel_allocator_malloc(
            encoder->allocator, mapfile_full_length);
        if (mapfile_normalized == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: sixel_allocator_malloc() failed "
                "for mapfile normalization.");
            sixel_allocator_free(encoder->allocator, libc_buffer);
            sixel_allocator_free(encoder->allocator, mapfile_copy);
            return SIXEL_BAD_ALLOCATION;
        }

        memcpy(mapfile_normalized, mapfile_copy, mapfile_offset);
        memcpy(mapfile_normalized + mapfile_offset,
               libc_path,
               libc_buffer_size);
        sixel_allocator_free(encoder->allocator, libc_buffer);
        sixel_allocator_free(encoder->allocator, mapfile_copy);
        mapfile_copy = mapfile_normalized;
        mapfile_normalized = NULL;
        mapfile_copy_view = mapfile_copy + mapfile_offset;
    }

    path_flags = SIXEL_OPTION_PATH_ALLOW_STDIN
        | SIXEL_OPTION_PATH_ALLOW_CLIPBOARD
        | SIXEL_OPTION_PATH_ALLOW_REMOTE
        | SIXEL_OPTION_PATH_ALLOW_EMPTY;
    path_check = sixel_option_validate_filesystem_path(value,
                                                       mapfile_copy_view,
                                                       path_flags);
    if (path_check != 0) {
        sixel_allocator_free(encoder->allocator, mapfile_copy);
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_encoder_check_palette_option_conflict(
        encoder,
        SIXEL_COLOR_OPTION_MAPFILE,
        "-m, --mapfile");
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(encoder->allocator, mapfile_copy);
        return status;
    }

    if (encoder->mapfile != NULL) {
        sixel_allocator_free(encoder->allocator, encoder->mapfile);
    }
    encoder->mapfile = mapfile_copy;
    encoder->color_option = SIXEL_COLOR_OPTION_MAPFILE;
    return status;
}


static SIXELSTATUS
sixel_encoder_apply_mapfile_output_option(
    sixel_encoder_t *encoder,
    char const *value)
{
    SIXELSTATUS status;
    char *opt_copy;

    status = SIXEL_OK;
    opt_copy = NULL;
    if (value == NULL || *value == '\0') {
        sixel_helper_set_additional_message(
            "sixel_encoder_setopt: mapfile-output path is empty.");
        return SIXEL_BAD_ARGUMENT;
    }

    opt_copy = arg_strdup(value, encoder->allocator);
    if (opt_copy == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_setopt: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    status = sixel_encoder_enable_quantized_capture_internal(encoder, 1);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(encoder->allocator, opt_copy);
        return status;
    }

    sixel_allocator_free(encoder->allocator, encoder->palette_output);
    encoder->palette_output = opt_copy;
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_encoder_apply_outfile_option(
    sixel_encoder_t *encoder,
    char const *value)
{
    SIXELSTATUS status;
    sixel_clipboard_spec_t clipboard_spec;
    SIXELSTATUS clip_status;
    char *spool_path;
    int spool_fd;
    int output_open_flags;
    int png_argument_has_prefix;
    size_t png_path_length;
    size_t libc_buffer_size;
    char *libc_buffer;
    char const *png_path_view;
    char const *outfile_open_path;
    char const *libc_path;

    status = SIXEL_OK;
    clip_status = SIXEL_OK;
    spool_path = NULL;
    spool_fd = (-1);
    output_open_flags = 0;
    png_argument_has_prefix = 0;
    png_path_length = 0u;
    libc_buffer_size = 0u;
    libc_buffer = NULL;
    png_path_view = NULL;
    outfile_open_path = value;
    libc_path = NULL;
    clipboard_spec.is_clipboard = 0;
    clipboard_spec.format[0] = '\0';
    if (value == NULL || *value == '\0') {
        sixel_helper_set_additional_message(
            "no file name specified.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (is_png_target(value)) {
        encoder->output_is_png = 1;
        png_argument_has_prefix = (strncmp(value, "png:", 4) == 0);
        png_path_view = png_target_payload_view(value);
        if (png_argument_has_prefix
            && (png_path_view == NULL || png_path_view[0] == '\0')) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: missing target after the \"png:\" "
                "prefix. use png:- or png:<path> with a non-empty payload."
            );
            return SIXEL_BAD_ARGUMENT;
        }
        encoder->output_png_to_stdout =
            (png_path_view != NULL) && (strcmp(png_path_view, "-") == 0);
        sixel_allocator_free(encoder->allocator, encoder->png_output_path);
        encoder->png_output_path = NULL;
        sixel_allocator_free(encoder->allocator, encoder->sixel_output_path);
        encoder->sixel_output_path = NULL;
        if (!encoder->output_png_to_stdout) {
            /*
             * Store only the payload path so downstream writers never have to
             * parse the "png:" pseudo-scheme.
             */
            png_path_view = value;
            if (strncmp(value, "png:", 4) == 0) {
                png_path_view = value + 4;
            }
            if (png_path_view[0] == '\0') {
                sixel_helper_set_additional_message(
                    "sixel_encoder_setopt: PNG output path is empty.");
                return SIXEL_BAD_ARGUMENT;
            }
            png_path_length = strlen(png_path_view);
            encoder->png_output_path = (char *)sixel_allocator_malloc(
                encoder->allocator, png_path_length + 1u);
            if (encoder->png_output_path == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_setopt: sixel_allocator_malloc() "
                    "failed for PNG output path.");
                return SIXEL_BAD_ALLOCATION;
            }
            (void)sixel_compat_strcpy(encoder->png_output_path,
                                      png_path_length + 1u,
                                      png_path_view);
            libc_buffer_size = sixel_path_to_libc_buffer_size(
                encoder->png_output_path);
            if (libc_buffer_size > 0u) {
                libc_buffer = (char *)sixel_allocator_malloc(
                    encoder->allocator, libc_buffer_size);
                if (libc_buffer == NULL) {
                    sixel_helper_set_additional_message(
                        "sixel_encoder_setopt: sixel_allocator_malloc() "
                        "failed for PNG path buffer.");
                    return SIXEL_BAD_ALLOCATION;
                }
                libc_path = sixel_path_to_libc(encoder->png_output_path,
                                               libc_buffer,
                                               libc_buffer_size);
                if (libc_path == NULL) {
                    sixel_helper_set_additional_message(
                        "sixel_encoder_setopt: invalid PNG output path.");
                    sixel_allocator_free(encoder->allocator, libc_buffer);
                    return SIXEL_BAD_ARGUMENT;
                }
                if (libc_path == libc_buffer) {
                    sixel_allocator_free(encoder->allocator,
                                         encoder->png_output_path);
                    encoder->png_output_path = libc_buffer;
                    libc_buffer = NULL;
                }
                if (libc_buffer != NULL) {
                    sixel_allocator_free(encoder->allocator, libc_buffer);
                    libc_buffer = NULL;
                }
            }
        }
        outfile_open_path = encoder->output_png_to_stdout
            ? "-"
            : encoder->png_output_path;
    } else {
        encoder->output_is_png = 0;
        encoder->output_png_to_stdout = 0;
        sixel_allocator_free(encoder->allocator, encoder->png_output_path);
        encoder->png_output_path = NULL;
        sixel_allocator_free(encoder->allocator, encoder->sixel_output_path);
        encoder->sixel_output_path = NULL;
        if (encoder->clipboard_output_path != NULL) {
            (void)sixel_compat_unlink(encoder->clipboard_output_path);
            sixel_allocator_free(encoder->allocator,
                                 encoder->clipboard_output_path);
            encoder->clipboard_output_path = NULL;
        }
        encoder->clipboard_output_active = 0;
        encoder->clipboard_output_format[0] = '\0';

        if (sixel_clipboard_parse_spec(value, &clipboard_spec)
            && clipboard_spec.is_clipboard) {
            clip_status = clipboard_create_spool(encoder->allocator,
                                                 "clipboard-out",
                                                 &spool_path,
                                                 &spool_fd);
            if (SIXEL_FAILED(clip_status)) {
                return clip_status;
            }
            clipboard_select_format(encoder->clipboard_output_format,
                                    sizeof(encoder->clipboard_output_format),
                                    clipboard_spec.format,
                                    "sixel");
            if (encoder->outfd
                && encoder->outfd != STDOUT_FILENO
                && encoder->outfd != STDERR_FILENO) {
                (void)sixel_compat_close(encoder->outfd);
            }
            encoder->outfd = spool_fd;
            spool_fd = (-1);
            encoder->sixel_output_path = spool_path;
            encoder->clipboard_output_path = spool_path;
            spool_path = NULL;
            encoder->clipboard_output_active = 1;
            return SIXEL_OK;
        }

        if (spool_fd >= 0) {
            (void)sixel_compat_close(spool_fd);
            spool_fd = (-1);
        }
        if (spool_path != NULL) {
            sixel_allocator_free(encoder->allocator, spool_path);
            spool_path = NULL;
        }

        if (strcmp(value, "-") != 0) {
            encoder->sixel_output_path = (char *)sixel_allocator_malloc(
                encoder->allocator, strlen(value) + 1u);
            if (encoder->sixel_output_path == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_setopt: malloc() failed for output path.");
                return SIXEL_BAD_ALLOCATION;
            }
            (void)sixel_compat_strcpy(encoder->sixel_output_path,
                                      strlen(value) + 1u,
                                      value);
        }
        outfile_open_path = value;
    }

    /*
     * Keep "png:" handling in the option layer so the encode phase can
     * assume filesystem paths or stdout sentinels only.
     */
    if (!encoder->clipboard_output_active
        && !encoder->output_is_png
        && outfile_open_path != NULL
        && strcmp(outfile_open_path, "-") != 0) {
        if (encoder->outfd && encoder->outfd != STDOUT_FILENO) {
            (void)sixel_compat_close(encoder->outfd);
        }
        output_open_flags = O_RDWR | O_CREAT | O_TRUNC;
#if defined(O_BINARY)
        output_open_flags |= O_BINARY;
#endif
        encoder->outfd = sixel_compat_open(outfile_open_path,
                                           output_open_flags,
                                           S_IRUSR | S_IWUSR);
    }

    return status;
}


static SIXELSTATUS
sixel_encoder_apply_drcs_option(
    sixel_encoder_t *encoder,
    char const *value)
{
    SIXELSTATUS status;
    char const *drcs_arg_delim;
    char const *drcs_arg_charset;
    char const *drcs_arg_second_delim;
    char const *drcs_arg_path;
    size_t drcs_arg_path_length;
    size_t drcs_segment_length;
    char drcs_segment[32];
    char *endptr;
    int drcs_mmv_value;
    int tile_open_flags;
    long drcs_charset_value;
    unsigned int drcs_charset_limit;

    status = SIXEL_OK;
    drcs_arg_delim = NULL;
    drcs_arg_charset = NULL;
    drcs_arg_second_delim = NULL;
    drcs_arg_path = NULL;
    drcs_arg_path_length = 0u;
    drcs_segment_length = 0u;
    endptr = NULL;
    drcs_mmv_value = 2;
    tile_open_flags = 0;
    drcs_charset_value = 1L;
    drcs_charset_limit = 0u;
    encoder->fdrcs = 1;

    if (value != NULL && *value != '\0') {
        drcs_arg_delim = strchr(value, ':');
        if (drcs_arg_delim == NULL) {
            drcs_segment_length = strlen(value);
            if (drcs_segment_length >= sizeof(drcs_segment)) {
                sixel_helper_set_additional_message(
                    "DRCS mapping revision is too long.");
                return SIXEL_BAD_ARGUMENT;
            }
            memcpy(drcs_segment, value, drcs_segment_length);
            drcs_segment[drcs_segment_length] = '\0';
            errno = 0;
            endptr = NULL;
            drcs_mmv_value = (int)strtol(drcs_segment, &endptr, 10);
            if (errno != 0 || endptr == drcs_segment || *endptr != '\0') {
                sixel_helper_set_additional_message(
                    "cannot parse DRCS option.");
                return SIXEL_BAD_ARGUMENT;
            }
        } else {
            if (drcs_arg_delim != value) {
                drcs_segment_length = (size_t)(drcs_arg_delim - value);
                if (drcs_segment_length >= sizeof(drcs_segment)) {
                    sixel_helper_set_additional_message(
                        "DRCS mapping revision is too long.");
                    return SIXEL_BAD_ARGUMENT;
                }
                memcpy(drcs_segment, value, drcs_segment_length);
                drcs_segment[drcs_segment_length] = '\0';
                errno = 0;
                endptr = NULL;
                drcs_mmv_value = (int)strtol(drcs_segment, &endptr, 10);
                if (errno != 0 || endptr == drcs_segment
                    || *endptr != '\0') {
                    sixel_helper_set_additional_message(
                        "cannot parse DRCS option.");
                    return SIXEL_BAD_ARGUMENT;
                }
            }
            drcs_arg_charset = drcs_arg_delim + 1;
            drcs_arg_second_delim = strchr(drcs_arg_charset, ':');
            if (drcs_arg_second_delim != NULL) {
                if (drcs_arg_second_delim != drcs_arg_charset) {
                    drcs_segment_length =
                        (size_t)(drcs_arg_second_delim - drcs_arg_charset);
                    if (drcs_segment_length >= sizeof(drcs_segment)) {
                        sixel_helper_set_additional_message(
                            "DRCS charset number is too long.");
                        return SIXEL_BAD_ARGUMENT;
                    }
                    memcpy(drcs_segment,
                           drcs_arg_charset,
                           drcs_segment_length);
                    drcs_segment[drcs_segment_length] = '\0';
                    errno = 0;
                    endptr = NULL;
                    drcs_charset_value = strtol(drcs_segment, &endptr, 10);
                    if (errno != 0 || endptr == drcs_segment
                        || *endptr != '\0') {
                        sixel_helper_set_additional_message(
                            "cannot parse DRCS charset number.");
                        return SIXEL_BAD_ARGUMENT;
                    }
                }
                drcs_arg_path = drcs_arg_second_delim + 1;
                drcs_arg_path_length = strlen(drcs_arg_path);
                if (drcs_arg_path_length == 0u) {
                    drcs_arg_path = NULL;
                }
            } else if (*drcs_arg_charset != '\0') {
                drcs_segment_length = strlen(drcs_arg_charset);
                if (drcs_segment_length >= sizeof(drcs_segment)) {
                    sixel_helper_set_additional_message(
                        "DRCS charset number is too long.");
                    return SIXEL_BAD_ARGUMENT;
                }
                memcpy(drcs_segment, drcs_arg_charset, drcs_segment_length);
                drcs_segment[drcs_segment_length] = '\0';
                errno = 0;
                endptr = NULL;
                drcs_charset_value = strtol(drcs_segment, &endptr, 10);
                if (errno != 0 || endptr == drcs_segment
                    || *endptr != '\0') {
                    sixel_helper_set_additional_message(
                        "cannot parse DRCS charset number.");
                    return SIXEL_BAD_ARGUMENT;
                }
            }
        }
    }

    if (drcs_mmv_value < 0 || drcs_mmv_value > 3) {
        sixel_helper_set_additional_message(
            "unknown DRCS unicode mapping version.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (drcs_mmv_value == 0) {
        drcs_charset_limit = 126u;
    } else if (drcs_mmv_value == 1) {
        drcs_charset_limit = 63u;
    } else if (drcs_mmv_value == 2) {
        drcs_charset_limit = 158u;
    } else {
        drcs_charset_limit = 697u;
    }
    if (drcs_charset_value < 1
        || (unsigned long)drcs_charset_value > drcs_charset_limit) {
        sixel_helper_set_additional_message(
            "DRCS charset number is out of range.");
        return SIXEL_BAD_ARGUMENT;
    }

    encoder->drcs_mmv = drcs_mmv_value;
    encoder->drcs_charset_no = (unsigned short)drcs_charset_value;
    if (encoder->tile_outfd >= 0
        && encoder->tile_outfd != encoder->outfd
        && encoder->tile_outfd != STDOUT_FILENO
        && encoder->tile_outfd != STDERR_FILENO) {
        (void)sixel_compat_close(encoder->tile_outfd);
    }
    encoder->tile_outfd = (-1);
    if (drcs_arg_path != NULL) {
        if (strcmp(drcs_arg_path, "-") == 0) {
            encoder->tile_outfd = STDOUT_FILENO;
        } else {
            tile_open_flags = O_RDWR | O_CREAT | O_TRUNC;
#if defined(O_BINARY)
            tile_open_flags |= O_BINARY;
#endif
            encoder->tile_outfd = sixel_compat_open(drcs_arg_path,
                                                    tile_open_flags,
                                                    S_IRUSR | S_IWUSR);
            if (encoder->tile_outfd < 0) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_setopt: failed to open tile"
                    " output path.");
                return SIXEL_RUNTIME_ERROR;
            }
        }
    }

    return status;
}

/* set an option flag to encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_setopt(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ arg,
    char const      /* in */ *value)
{
    SIXELSTATUS status = SIXEL_FALSE;
    char lowered[16];
    size_t len;
    size_t i;
    sixel_encoder_setopt_context_t setopt_context;
    int match_value;
    sixel_option_argument_resolution_t d_resolution;
    sixel_encoder_interframe_suboption_state_t interframe_suboptions;
    sixel_encoder_bluenoise_suboption_state_t bluenoise_suboptions;
    int diffusion_scan;
    sixel_option_argument_resolution_t const *q_resolution;
    size_t q_index;
    double q_threshold;
    double q_merge_oversplit;
    unsigned int q_binbits;
    unsigned int q_autoratio;
    unsigned int q_restarts;
    unsigned int q_init_seeds;
    uint32_t q_seed;
    unsigned int q_iter;
    unsigned int q_iter_max;
    unsigned int q_miniter;
    unsigned int q_polish_iter;
    unsigned int q_feedback_slots;
    unsigned int q_feedback_interval;
    unsigned int q_merge_lloyd;
    unsigned int q_sample;
    unsigned int q_clara_trials;
    unsigned int q_clara_sample;
    unsigned int q_clarans_local;
    unsigned int q_clarans_neighbors;
    unsigned int q_bandit_iter;
    unsigned int q_bandit_candidates;
    unsigned int q_bandit_batch;
    unsigned int q_histbits;
    unsigned int q_point_budget;
    unsigned int q_auto_fft_threshold;
    unsigned int q_rare_keep;
    unsigned int q_swap_topk;
    unsigned int q_swap_patience;
    double q_swap_min_gain;
    double q_prune_mass;
    double q_budget_scale;
    int q_auction;
    unsigned int q_auction_shortlist;
    int q_animation_mode;
    double q_scene_cut_threshold;
    int q_heckbert_profile;
    sixel_suboption_assignment_t const *q_assignment;
    char const *q_key;
    int q_model;
    int lut_shared_instance_override;
    int lut_shared_instance_value;
    char match_detail[128];

    sixel_encoder_ref(encoder);
    sixel_encoder_setopt_context_init(&setopt_context);
    d_resolution.resolved_base_value = 0;
    d_resolution.base_def = NULL;
    d_resolution.assignments = NULL;
    d_resolution.assignment_count = 0u;
    interframe_suboptions.has_strategy_override = 0;
    interframe_suboptions.strategy_token = SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE;
    interframe_suboptions.has_spatial_diffuse_override = 0;
    interframe_suboptions.spatial_diffuse = SIXEL_DIFFUSE_FS;
    interframe_suboptions.has_noise_strength_override = 0;
    interframe_suboptions.noise_strength_u8 = 0;
    interframe_suboptions.has_motion_adapt_override = 0;
    interframe_suboptions.motion_adapt_enabled = 0;
    interframe_suboptions.has_scene_cut_reset_override = 0;
    interframe_suboptions.scene_cut_reset_enabled = 0;
    interframe_suboptions.has_scene_detect_override = 0;
    interframe_suboptions.scene_detect_enabled = 0;
    interframe_suboptions.has_alpha_guard_override = 0;
    interframe_suboptions.alpha_guard_enabled = 0;
    interframe_suboptions.has_perceptual_weight_override = 0;
    interframe_suboptions.perceptual_weight_enabled = 0;
    interframe_suboptions.has_fastpath_override = 0;
    interframe_suboptions.fastpath_enabled = 0;
    bluenoise_suboptions.has_strength_override = 0;
    bluenoise_suboptions.strength = 0.055f;
    bluenoise_suboptions.has_gradient_factor_override = 0;
    bluenoise_suboptions.gradient_factor = 0.0f;
    bluenoise_suboptions.has_phase_override = 0;
    bluenoise_suboptions.phase_x = 0;
    bluenoise_suboptions.phase_y = 0;
    bluenoise_suboptions.has_seed_override = 0;
    bluenoise_suboptions.seed = 0;
    bluenoise_suboptions.has_channel_override = 0;
    bluenoise_suboptions.channel_rgb = 0;
    bluenoise_suboptions.has_size_override = 0;
    bluenoise_suboptions.size = 64;
    diffusion_scan = SIXEL_SCAN_AUTO;
    q_resolution = NULL;
    q_index = 0u;
    q_threshold = 0.0;
    q_merge_oversplit = 0.0;
    q_binbits = 0u;
    q_autoratio = 0u;
    q_restarts = 0u;
    q_init_seeds = 0u;
    q_seed = 0u;
    q_iter = 0u;
    q_iter_max = 0u;
    q_miniter = 0u;
    q_polish_iter = 0u;
    q_feedback_slots = 0u;
    q_feedback_interval = 0u;
    q_merge_lloyd = 0u;
    q_sample = 0u;
    q_clara_trials = 0u;
    q_clara_sample = 0u;
    q_clarans_local = 0u;
    q_clarans_neighbors = 0u;
    q_bandit_iter = 0u;
    q_bandit_candidates = 0u;
    q_bandit_batch = 0u;
    q_histbits = 0u;
    q_point_budget = 0u;
    q_auto_fft_threshold = 0u;
    q_rare_keep = 0u;
    q_swap_topk = 0u;
    q_swap_patience = 0u;
    q_swap_min_gain = 0.0;
    q_prune_mass = 0.0;
    q_budget_scale = 0.0;
    q_auction = 0;
    q_auction_shortlist = 0u;
    q_animation_mode = 0;
    q_scene_cut_threshold = 0.0;
    q_heckbert_profile = SIXEL_HECKBERT_PROFILE_COMPAT;
    q_assignment = NULL;
    q_key = NULL;
    q_model = SIXEL_QUANTIZE_MODEL_AUTO;
    lut_shared_instance_override = 0;
    lut_shared_instance_value = 0;

    switch(arg) {
    case SIXEL_OPTFLAG_OUTFILE:  /* o */
        status = sixel_encoder_apply_outfile_option(encoder, value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_7BIT_MODE:  /* 7 */
        encoder->f8bit = 0;
        break;
    case SIXEL_OPTFLAG_8BIT_MODE:  /* 8 */
        encoder->f8bit = 1;
        break;
    case SIXEL_OPTFLAG_6REVERSIBLE:  /* 6 */
        encoder->sixel_reversible = 1;
        break;
    case SIXEL_OPTFLAG_HAS_GRI_ARG_LIMIT:  /* R */
        encoder->has_gri_arg_limit = 1;
        break;
    case SIXEL_OPTFLAG_PRECISION:  /* . */
        status = sixel_encoder_parse_choice_argument(
            value,
            g_option_choices_precision,
            sizeof(g_option_choices_precision) /
                sizeof(g_option_choices_precision[0]),
            "precision accepts auto, 8bit, or float32.",
            &match_value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_encoder_apply_precision_override(
            encoder,
            (sixel_encoder_precision_mode_t)match_value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_THREADS:  /* = */
        status = sixel_encoder_apply_threads_option(value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_COLORS:  /* p */
        status = sixel_encoder_apply_colors_option(encoder, value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_MAPFILE:  /* m */
        status = sixel_encoder_apply_mapfile_option(encoder, value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_MAPFILE_OUTPUT:  /* M */
        status = sixel_encoder_apply_mapfile_output_option(encoder, value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_MONOCHROME:  /* e */
        status = sixel_encoder_check_palette_option_conflict(
            encoder,
            SIXEL_COLOR_OPTION_MONOCHROME,
            "-e, --monochrome");
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        encoder->color_option = SIXEL_COLOR_OPTION_MONOCHROME;
        break;
    case SIXEL_OPTFLAG_HIGH_COLOR:  /* I */
        status = sixel_encoder_check_palette_option_conflict(
            encoder,
            SIXEL_COLOR_OPTION_HIGHCOLOR,
            "-I, --high-color");
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        encoder->color_option = SIXEL_COLOR_OPTION_HIGHCOLOR;
        break;
    case SIXEL_OPTFLAG_BUILTIN_PALETTE:  /* b */
        status = sixel_encoder_parse_choice_argument(
            value,
            g_option_choices_builtin_palette,
            sizeof(g_option_choices_builtin_palette) /
                sizeof(g_option_choices_builtin_palette[0]),
            "cannot parse builtin palette option.",
            &match_value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_encoder_check_palette_option_conflict(
            encoder,
            SIXEL_COLOR_OPTION_BUILTIN,
            "-b, --builtin-palette");
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        encoder->builtin_palette = match_value;
        encoder->color_option = SIXEL_COLOR_OPTION_BUILTIN;
        break;
    case SIXEL_OPTFLAG_DIFFUSION:  /* d */
        status = sixel_encoder_parse_diffusion_argument(
            value,
            &d_resolution,
            match_detail,
            sizeof(match_detail));
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        match_value = d_resolution.resolved_base_value;
        status = SIXEL_OK;
        status = sixel_encoder_resolve_sierra_suboptions(
            &d_resolution,
            &match_value);
        if (SIXEL_FAILED(status)) {
            sixel_option_free_argument_resolution(&d_resolution);
            goto end;
        }
        status = sixel_encoder_resolve_diffusion_scan_suboptions(
            &d_resolution,
            &diffusion_scan);
        if (SIXEL_FAILED(status)) {
            sixel_option_free_argument_resolution(&d_resolution);
            goto end;
        }
        status = sixel_encoder_resolve_interframe_suboptions(
            &d_resolution,
            &interframe_suboptions);
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_encoder_resolve_bluenoise_suboptions(
                &d_resolution,
                &bluenoise_suboptions);
        }
        sixel_option_free_argument_resolution(&d_resolution);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        encoder->method_for_diffuse = match_value;
        encoder->method_for_scan = diffusion_scan;
        encoder->interframe_strategy_override =
            interframe_suboptions.has_strategy_override;
        encoder->interframe_strategy_token =
            interframe_suboptions.strategy_token;
        encoder->interframe_spatial_diffuse_override
            = interframe_suboptions.has_spatial_diffuse_override;
        encoder->interframe_spatial_diffuse =
            interframe_suboptions.spatial_diffuse;
        encoder->interframe_noise_strength_override
            = interframe_suboptions.has_noise_strength_override;
        encoder->interframe_noise_strength_u8 =
            interframe_suboptions.noise_strength_u8;
        encoder->stbn_motion_adapt_override =
            interframe_suboptions.has_motion_adapt_override;
        encoder->stbn_motion_adapt_enabled =
            interframe_suboptions.motion_adapt_enabled;
        encoder->stbn_scene_cut_reset_override =
            interframe_suboptions.has_scene_cut_reset_override;
        encoder->stbn_scene_cut_reset_enabled =
            interframe_suboptions.scene_cut_reset_enabled;
        encoder->stbn_scene_detect_override =
            interframe_suboptions.has_scene_detect_override;
        encoder->stbn_scene_detect_enabled =
            interframe_suboptions.scene_detect_enabled;
        encoder->stbn_alpha_guard_override =
            interframe_suboptions.has_alpha_guard_override;
        encoder->stbn_alpha_guard_enabled =
            interframe_suboptions.alpha_guard_enabled;
        encoder->stbn_perceptual_weight_override
            = interframe_suboptions.has_perceptual_weight_override;
        encoder->stbn_perceptual_weight_enabled
            = interframe_suboptions.perceptual_weight_enabled;
        encoder->stbn_fastpath_override =
            interframe_suboptions.has_fastpath_override;
        encoder->stbn_fastpath_enabled = interframe_suboptions.fastpath_enabled;
        encoder->bluenoise_strength_override =
            bluenoise_suboptions.has_strength_override;
        encoder->bluenoise_strength = bluenoise_suboptions.strength;
        encoder->bluenoise_gradient_factor_override =
            bluenoise_suboptions.has_gradient_factor_override;
        encoder->bluenoise_gradient_factor =
            bluenoise_suboptions.gradient_factor;
        encoder->bluenoise_phase_override =
            bluenoise_suboptions.has_phase_override;
        encoder->bluenoise_phase_x = bluenoise_suboptions.phase_x;
        encoder->bluenoise_phase_y = bluenoise_suboptions.phase_y;
        encoder->bluenoise_seed_override =
            bluenoise_suboptions.has_seed_override;
        encoder->bluenoise_seed = bluenoise_suboptions.seed;
        encoder->bluenoise_channel_override =
            bluenoise_suboptions.has_channel_override;
        encoder->bluenoise_channel_rgb = bluenoise_suboptions.channel_rgb;
        encoder->bluenoise_size_override =
            bluenoise_suboptions.has_size_override;
        encoder->bluenoise_size = bluenoise_suboptions.size;
        break;
    case SIXEL_OPTFLAG_FIND_LARGEST:  /* f */
        if (value != NULL) {
            status = sixel_encoder_parse_choice_argument(
                value,
                g_option_choices_find_largest,
                sizeof(g_option_choices_find_largest) /
                sizeof(g_option_choices_find_largest[0]),
                "specified finding method is not supported.",
                &match_value);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            encoder->method_for_largest = match_value;
            encoder->method_for_largest_override = 1;
        }
        break;
    case SIXEL_OPTFLAG_SELECT_COLOR:  /* s */
        status = sixel_encoder_parse_choice_argument(
            value,
            g_option_choices_select_color,
            sizeof(g_option_choices_select_color) /
                sizeof(g_option_choices_select_color[0]),
            "specified finding method is not supported.",
            &match_value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        encoder->method_for_rep = match_value;
        break;
    case SIXEL_OPTFLAG_QUANTIZE_MODEL:  /* Q */
        /*
         * Parse + validate quantize-model options through the list parser so
         * canonicalization and diagnostics follow the same AST lifecycle as
         * loader-order parsing.
         */
        status = sixel_encoder_parse_quantize_model_argument(
            value,
            &setopt_context.q_list_resolution,
            match_detail,
            sizeof(match_detail));
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        q_resolution = &setopt_context.q_list_resolution.items[0].resolution;
        q_model = q_resolution->resolved_base_value;
        encoder->quantize_model = q_model;
        encoder->quantize_model_kmeans_init_override = 0;
        encoder->quantize_model_kmeans_threshold_override = 0;
        encoder->quantize_model_kmeans_binning_override = 0;
        encoder->quantize_model_kmeans_binbits_override = 0;
        encoder->quantize_model_kmeans_mapping_override = 0;
        encoder->quantize_model_kmeans_softdist_override = 0;
        encoder->quantize_model_kmeans_autoratio_override = 0;
        encoder->quantize_model_kmeans_feedback_override = 0;
        encoder->quantize_model_kmeans_prune_override = 0;
        encoder->quantize_model_kmeans_seed_override = 0;
        encoder->quantize_model_kmeans_restarts_override = 0;
        encoder->quantize_model_kmeans_iter_override = 0;
        encoder->quantize_model_kmeans_iter_max_override = 0;
        encoder->quantize_model_kmeans_miniter_override = 0;
        encoder->quantize_model_kmeans_polish_iter_override = 0;
        encoder->quantize_model_kmeans_feedback_slots_override = 0;
        encoder->quantize_model_kmeans_feedback_interval_override = 0;
        encoder->quantize_model_kmedoids_algo_override = 0;
        encoder->quantize_model_kmedoids_seed_override = 0;
        encoder->quantize_model_kmedoids_iter_override = 0;
        encoder->quantize_model_kmedoids_sample_override = 0;
        encoder->quantize_model_kmedoids_clara_trials_override = 0;
        encoder->quantize_model_kmedoids_clara_sample_override = 0;
        encoder->quantize_model_kmedoids_clarans_local_override = 0;
        encoder->quantize_model_kmedoids_clarans_neighbors_override = 0;
        encoder->quantize_model_kmedoids_bandit_iter_override = 0;
        encoder->quantize_model_kmedoids_bandit_candidates_override = 0;
        encoder->quantize_model_kmedoids_bandit_batch_override = 0;
        encoder->quantize_model_kmedoids_histbits_override = 0;
        encoder->quantize_model_kmedoids_point_budget_override = 0;
        encoder->quantize_model_kmedoids_rare_keep_override = 0;
        encoder->quantize_model_kmedoids_prune_mass_override = 0;
        encoder->quantize_model_kmedoids_auction_override = 0;
        encoder->quantize_model_kmedoids_auction_shortlist_override = 0;
        encoder->quantize_model_kcenter_algo_override = 0;
        encoder->quantize_model_kcenter_seed_override = 0;
        encoder->quantize_model_kcenter_restarts_override = 0;
        encoder->quantize_model_kcenter_init_seeds_override = 0;
        encoder->quantize_model_kcenter_iter_override = 0;
        encoder->quantize_model_kcenter_histbits_override = 0;
        encoder->quantize_model_kcenter_point_budget_override = 0;
        encoder->quantize_model_kcenter_prune_mass_override = 0;
        encoder->quantize_model_kcenter_profile_override = 0;
        encoder->quantize_model_kcenter_auto_policy_override = 0;
        encoder->quantize_model_kcenter_auto_fft_threshold_override = 0;
        encoder->quantize_model_kcenter_space_policy_override = 0;
        encoder->quantize_model_kcenter_candidate_policy_override = 0;
        encoder->quantize_model_kcenter_rare_keep_override = 0;
        encoder->quantize_model_kcenter_budget_policy_override = 0;
        encoder->quantize_model_kcenter_budget_scale_override = 0;
        encoder->quantize_model_kcenter_swap_topk_override = 0;
        encoder->quantize_model_kcenter_swap_update_override = 0;
        encoder->quantize_model_kcenter_swap_patience_override = 0;
        encoder->quantize_model_kcenter_swap_min_gain_override = 0;
        encoder->quantize_model_merge_override = 0;
        encoder->quantize_model_merge_oversplit_override = 0;
        encoder->quantize_model_merge_lloyd_override = 0;
        encoder->quantize_model_animation_mode_override = 0;
        encoder->quantize_model_scene_cut_threshold_override = 0;
        encoder->quantize_model_heckbert_profile
            = SIXEL_HECKBERT_PROFILE_COMPAT;

        q_index = 0u;
        while (q_index < q_resolution->assignment_count) {
            q_assignment = q_resolution->assignments + q_index;
            q_key = q_assignment->resolved_key_name;
            if (q_key == NULL) {
                ++q_index;
                continue;
            }
            if (q_key != NULL && strcmp(q_key, "animation_mode") == 0) {
                status = sixel_encoder_parse_quantize_animation_mode_text(
                    q_assignment->resolved_value_text,
                    &q_animation_mode);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_animation_mode_override = 1;
                encoder->quantize_model_animation_mode = q_animation_mode;
            } else if (q_key != NULL
                    && strcmp(q_key, "scene_cut_threshold") == 0) {
                status = sixel_encoder_parse_quantize_scene_cut_threshold_text(
                    q_assignment->resolved_value_text,
                    &q_scene_cut_threshold);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_scene_cut_threshold_override = 1;
                encoder->quantize_model_scene_cut_threshold
                    = q_scene_cut_threshold;
            } else if (q_key != NULL && strcmp(q_key, "inittype") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q inittype resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_init_override = 1;
                encoder->quantize_model_kmeans_init_type = match_value;
            } else if (q_key != NULL
                    && strcmp(q_key, "threshold") == 0) {
                status = sixel_encoder_parse_quantize_threshold_text(
                    q_assignment->resolved_value_text,
                    &q_threshold);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_threshold_override = 1;
                encoder->quantize_model_kmeans_threshold = q_threshold;
            } else if (q_key != NULL && strcmp(q_key, "binning") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q binning resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_binning_override = 1;
                encoder->quantize_model_kmeans_binning_mode = match_value;
            } else if (q_key != NULL && strcmp(q_key, "binbits") == 0) {
                status = sixel_encoder_parse_kmeans_binbits_text(
                    q_assignment->resolved_value_text,
                    &q_binbits);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_binbits_override = 1;
                encoder->quantize_model_kmeans_binbits = q_binbits;
            } else if (q_key != NULL && strcmp(q_key, "mapping") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q mapping resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_mapping_override = 1;
                encoder->quantize_model_kmeans_mapping_mode = match_value;
            } else if (q_key != NULL && strcmp(q_key, "softdist") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q softdist resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_softdist_override = 1;
                encoder->quantize_model_kmeans_softdist_mode = match_value;
            } else if (q_key != NULL && strcmp(q_key, "autoratio") == 0) {
                status = sixel_encoder_parse_kmeans_autoratio_text(
                    q_assignment->resolved_value_text,
                    &q_autoratio);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_autoratio_override = 1;
                encoder->quantize_model_kmeans_autoratio = q_autoratio;
            } else if (q_key != NULL && strcmp(q_key, "feedback") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q feedback resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_feedback_override = 1;
                encoder->quantize_model_kmeans_feedback_mode = match_value;
            } else if (q_key != NULL && strcmp(q_key, "prune") == 0
                    && q_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q prune resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_prune_override = 1;
                encoder->quantize_model_kmeans_prune_policy = match_value;
            } else if (q_key != NULL && strcmp(q_key, "algo") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q algo resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                if (q_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
                    encoder->quantize_model_kcenter_algo_override = 1;
                    encoder->quantize_model_kcenter_algo = match_value;
                } else {
                    encoder->quantize_model_kmedoids_algo_override = 1;
                    encoder->quantize_model_kmedoids_algo = match_value;
                }
            } else if (q_key != NULL && strcmp(q_key, "profile") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q profile resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                if (q_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
                    encoder->quantize_model_kcenter_profile_override = 1;
                    encoder->quantize_model_kcenter_profile = match_value;
                } else {
                    q_heckbert_profile = match_value;
                    encoder->quantize_model_heckbert_profile
                        = q_heckbert_profile;
                }
            } else if (q_key != NULL
                    && strcmp(q_key, "auto_policy") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q auto_policy resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kcenter_auto_policy_override = 1;
                encoder->quantize_model_kcenter_auto_policy = match_value;
            } else if (q_key != NULL
                    && strcmp(q_key, "auto_fft_threshold") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    256u,
                    65536u,
                    0,
                    "auto_fft_threshold",
                    &q_auto_fft_threshold);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kcenter_auto_fft_threshold_override = 1;
                encoder->quantize_model_kcenter_auto_fft_threshold
                    = q_auto_fft_threshold;
            } else if (q_key != NULL
                    && strcmp(q_key, "space_policy") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q space_policy resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kcenter_space_policy_override = 1;
                encoder->quantize_model_kcenter_space_policy = match_value;
            } else if (q_key != NULL
                    && strcmp(q_key, "candidate_policy") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q candidate_policy resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kcenter_candidate_policy_override = 1;
                encoder->quantize_model_kcenter_candidate_policy = match_value;
            } else if (q_key != NULL && strcmp(q_key, "seed") == 0) {
                status = sixel_encoder_parse_kmedoids_seed_text(
                    q_assignment->resolved_value_text,
                        &q_seed);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                if (q_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
                    encoder->quantize_model_kmeans_seed_override = 1;
                    encoder->quantize_model_kmeans_seed = q_seed;
                } else if (q_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
                    encoder->quantize_model_kcenter_seed_override = 1;
                    encoder->quantize_model_kcenter_seed = q_seed;
                } else {
                    encoder->quantize_model_kmedoids_seed_override = 1;
                    encoder->quantize_model_kmedoids_seed = q_seed;
                }
            } else if (q_key != NULL && strcmp(q_key, "iter") == 0) {
                if (q_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
                    status = sixel_encoder_parse_kmedoids_uint_text(
                        q_assignment->resolved_value_text,
                        1u,
                        100u,
                        0,
                        "iter",
                        &q_iter);
                } else {
                    status = sixel_encoder_parse_kmedoids_uint_text(
                        q_assignment->resolved_value_text,
                        1u,
                        64u,
                        0,
                        "iter",
                        &q_iter);
                }
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                if (q_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
                    encoder->quantize_model_kmeans_iter_override = 1;
                    encoder->quantize_model_kmeans_iter = q_iter;
                } else if (q_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
                    encoder->quantize_model_kcenter_iter_override = 1;
                    encoder->quantize_model_kcenter_iter = q_iter;
                } else {
                    encoder->quantize_model_kmedoids_iter_override = 1;
                    encoder->quantize_model_kmedoids_iter = q_iter;
                }
            } else if (q_key != NULL && strcmp(q_key, "iter_max") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    100u,
                    0,
                    "iter_max",
                    &q_iter_max);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_iter_max_override = 1;
                encoder->quantize_model_kmeans_iter_max = q_iter_max;
            } else if (q_key != NULL && strcmp(q_key, "miniter") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    100u,
                    1,
                    "miniter",
                    &q_miniter);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_miniter_override = 1;
                encoder->quantize_model_kmeans_miniter = q_miniter;
            } else if (q_key != NULL
                    && strcmp(q_key, "polish_iter") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    16u,
                    1,
                    "polish_iter",
                    &q_polish_iter);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_polish_iter_override = 1;
                encoder->quantize_model_kmeans_polish_iter = q_polish_iter;
            } else if (q_key != NULL && strcmp(q_key, "restarts") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    32u,
                    0,
                    "restarts",
                    &q_restarts);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                if (q_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
                    encoder->quantize_model_kcenter_restarts_override = 1;
                    encoder->quantize_model_kcenter_restarts = q_restarts;
                } else {
                    encoder->quantize_model_kmeans_restarts_override = 1;
                    encoder->quantize_model_kmeans_restarts = q_restarts;
                }
            } else if (q_key != NULL
                    && strcmp(q_key, "init_seeds") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    8u,
                    0,
                    "init_seeds",
                    &q_init_seeds);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kcenter_init_seeds_override = 1;
                encoder->quantize_model_kcenter_init_seeds = q_init_seeds;
            } else if (q_key != NULL
                    && strcmp(q_key, "feedback_slots") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    16u,
                    0,
                    "feedback_slots",
                    &q_feedback_slots);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_feedback_slots_override = 1;
                encoder->quantize_model_kmeans_feedback_slots = q_feedback_slots;
            } else if (q_key != NULL
                    && strcmp(q_key, "feedback_interval") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    64u,
                    0,
                    "feedback_interval",
                    &q_feedback_interval);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_feedback_interval_override = 1;
                encoder->quantize_model_kmeans_feedback_interval
                    = q_feedback_interval;
            } else if (q_key != NULL && strcmp(q_key, "sample") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    64u,
                    1048576u,
                    1,
                    "sample",
                    &q_sample);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmedoids_sample_override = 1;
                encoder->quantize_model_kmedoids_sample = q_sample;
            } else if (q_key != NULL
                    && strcmp(q_key, "clara_trials") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    32u,
                    0,
                    "clara_trials",
                    &q_clara_trials);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmedoids_clara_trials_override = 1;
                encoder->quantize_model_kmedoids_clara_trials = q_clara_trials;
            } else if (q_key != NULL
                    && strcmp(q_key, "clara_sample") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    64u,
                    1048576u,
                    1,
                    "clara_sample",
                    &q_clara_sample);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmedoids_clara_sample_override = 1;
                encoder->quantize_model_kmedoids_clara_sample = q_clara_sample;
            } else if (q_key != NULL
                    && strcmp(q_key, "clarans_local") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    32u,
                    0,
                    "clarans_local",
                    &q_clarans_local);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmedoids_clarans_local_override = 1;
                encoder->quantize_model_kmedoids_clarans_local
                    = q_clarans_local;
            } else if (q_key != NULL
                    && strcmp(q_key, "clarans_neighbors") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    5000000u,
                    1,
                    "clarans_neighbors",
                    &q_clarans_neighbors);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmedoids_clarans_neighbors_override = 1;
                encoder->quantize_model_kmedoids_clarans_neighbors
                    = q_clarans_neighbors;
            } else if (q_key != NULL
                    && strcmp(q_key, "bandit_iter") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    64u,
                    0,
                    "bandit_iter",
                    &q_bandit_iter);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmedoids_bandit_iter_override = 1;
                encoder->quantize_model_kmedoids_bandit_iter = q_bandit_iter;
            } else if (q_key != NULL
                    && strcmp(q_key, "bandit_candidates") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    8u,
                    4096u,
                    0,
                    "bandit_candidates",
                    &q_bandit_candidates);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmedoids_bandit_candidates_override
                    = 1;
                encoder->quantize_model_kmedoids_bandit_candidates
                    = q_bandit_candidates;
            } else if (q_key != NULL
                    && strcmp(q_key, "bandit_batch") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    8u,
                    4096u,
                    0,
                    "bandit_batch",
                    &q_bandit_batch);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmedoids_bandit_batch_override = 1;
                encoder->quantize_model_kmedoids_bandit_batch = q_bandit_batch;
            } else if (q_key != NULL && strcmp(q_key, "histbits") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    3u,
                    6u,
                    0,
                    "histbits",
                    &q_histbits);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                if (q_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
                    encoder->quantize_model_kcenter_histbits_override = 1;
                    encoder->quantize_model_kcenter_histbits = q_histbits;
                } else {
                    encoder->quantize_model_kmedoids_histbits_override = 1;
                    encoder->quantize_model_kmedoids_histbits = q_histbits;
                }
            } else if (q_key != NULL
                    && strcmp(q_key, "point_budget") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    64u,
                    16384u,
                    q_model == SIXEL_QUANTIZE_MODEL_KCENTER,
                    "point_budget",
                    &q_point_budget);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                if (q_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
                    encoder->quantize_model_kcenter_point_budget_override = 1;
                    encoder->quantize_model_kcenter_point_budget
                        = q_point_budget;
                } else {
                    encoder->quantize_model_kmedoids_point_budget_override = 1;
                    encoder->quantize_model_kmedoids_point_budget
                        = q_point_budget;
                }
            } else if (q_key != NULL && strcmp(q_key, "rare_keep") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    q_model == SIXEL_QUANTIZE_MODEL_KCENTER ? 1u : 0u,
                    q_model == SIXEL_QUANTIZE_MODEL_KCENTER ? 2048u : 1024u,
                    q_model == SIXEL_QUANTIZE_MODEL_KCENTER,
                    "rare_keep",
                    &q_rare_keep);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                if (q_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
                    encoder->quantize_model_kcenter_rare_keep_override = 1;
                    encoder->quantize_model_kcenter_rare_keep = q_rare_keep;
                } else {
                    encoder->quantize_model_kmedoids_rare_keep_override = 1;
                    encoder->quantize_model_kmedoids_rare_keep = q_rare_keep;
                }
            } else if (q_key != NULL
                    && strcmp(q_key, "budget_policy") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q budget_policy resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kcenter_budget_policy_override = 1;
                encoder->quantize_model_kcenter_budget_policy = match_value;
            } else if (q_key != NULL
                    && strcmp(q_key, "budget_scale") == 0) {
                status = sixel_encoder_parse_kcenter_budget_scale_text(
                    q_assignment->resolved_value_text,
                    &q_budget_scale);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kcenter_budget_scale_override = 1;
                encoder->quantize_model_kcenter_budget_scale
                    = q_budget_scale;
            } else if (q_key != NULL && strcmp(q_key, "swap_topk") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    16u,
                    0,
                    "swap_topk",
                    &q_swap_topk);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kcenter_swap_topk_override = 1;
                encoder->quantize_model_kcenter_swap_topk = q_swap_topk;
            } else if (q_key != NULL
                    && strcmp(q_key, "swap_update") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q swap_update resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kcenter_swap_update_override = 1;
                encoder->quantize_model_kcenter_swap_update = match_value;
            } else if (q_key != NULL
                    && strcmp(q_key, "swap_patience") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    8u,
                    1,
                    "swap_patience",
                    &q_swap_patience);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kcenter_swap_patience_override = 1;
                encoder->quantize_model_kcenter_swap_patience
                    = q_swap_patience;
            } else if (q_key != NULL
                    && strcmp(q_key, "swap_min_gain") == 0) {
                status = sixel_encoder_parse_kcenter_swap_min_gain_text(
                    q_assignment->resolved_value_text,
                    &q_swap_min_gain);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kcenter_swap_min_gain_override = 1;
                encoder->quantize_model_kcenter_swap_min_gain
                    = q_swap_min_gain;
            } else if (q_key != NULL && strcmp(q_key, "prune_mass") == 0) {
                status = sixel_encoder_parse_kmedoids_prune_mass_text(
                    q_assignment->resolved_value_text,
                    &q_prune_mass);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                if (q_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
                    encoder->quantize_model_kcenter_prune_mass_override = 1;
                    encoder->quantize_model_kcenter_prune_mass = q_prune_mass;
                } else {
                    encoder->quantize_model_kmedoids_prune_mass_override = 1;
                    encoder->quantize_model_kmedoids_prune_mass
                        = q_prune_mass;
                }
            } else if (q_key != NULL && strcmp(q_key, "auction") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &q_auction)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q auction resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmedoids_auction_override = 1;
                encoder->quantize_model_kmedoids_auction = q_auction;
            } else if (q_key != NULL
                    && strcmp(q_key, "auction_shortlist") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    2u,
                    8u,
                    0,
                    "auction_shortlist",
                    &q_auction_shortlist);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmedoids_auction_shortlist_override = 1;
                encoder->quantize_model_kmedoids_auction_shortlist
                    = q_auction_shortlist;
            } else if (q_key != NULL && strcmp(q_key, "merge") == 0) {
                if (!sixel_encoder_resolve_suboption_choice_value(
                        q_assignment,
                        &match_value)) {
                    sixel_helper_set_additional_message(
                        "invalid -Q merge resolution.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_merge_override = 1;
                encoder->quantize_model_merge_mode = match_value;
                encoder->final_merge_mode = match_value;
            } else if (q_key != NULL
                    && strcmp(q_key, "merge_oversplit") == 0) {
                status = sixel_encoder_parse_quantize_merge_oversplit_text(
                    q_assignment->resolved_value_text,
                    &q_merge_oversplit);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_merge_oversplit_override = 1;
                encoder->quantize_model_merge_oversplit = q_merge_oversplit;
            } else if (q_key != NULL
                    && strcmp(q_key, "merge_lloyd") == 0) {
                status = sixel_encoder_parse_kmedoids_uint_text(
                    q_assignment->resolved_value_text,
                    1u,
                    30u,
                    1,
                    "merge_lloyd",
                    &q_merge_lloyd);
                if (SIXEL_FAILED(status)) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_merge_lloyd_override = 1;
                encoder->quantize_model_merge_lloyd = q_merge_lloyd;
            }
            ++q_index;
        }
        sixel_encoder_apply_heckbert_profile_defaults(encoder);
        break;
    case SIXEL_OPTFLAG_CROP:  /* c */
        status = sixel_encoder_apply_crop_option(encoder, value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_WIDTH:  /* w */
        status = sixel_encoder_apply_width_option(encoder, value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_HEIGHT:  /* h */
        status = sixel_encoder_apply_height_option(encoder, value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_RESAMPLING:  /* r */
        status = sixel_encoder_parse_choice_argument(
            value,
            g_option_choices_resampling,
            sizeof(g_option_choices_resampling) /
            sizeof(g_option_choices_resampling[0]),
            "specified desampling method is not supported.",
            &match_value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        encoder->method_for_resampling = match_value;
        break;
    case SIXEL_OPTFLAG_QUALITY:  /* q */
        status = sixel_encoder_parse_choice_argument(
            value,
            g_option_choices_quality,
            sizeof(g_option_choices_quality) /
            sizeof(g_option_choices_quality[0]),
            "cannot parse quality option.",
            &match_value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        encoder->quality_mode = match_value;
        break;
    case SIXEL_OPTFLAG_LOOPMODE:  /* l */
        status = sixel_encoder_parse_choice_argument(
            value,
            g_option_choices_loopmode,
            sizeof(g_option_choices_loopmode) /
            sizeof(g_option_choices_loopmode[0]),
            "cannot parse loop-control option.",
            &match_value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        encoder->loop_mode = match_value;
        break;
    case SIXEL_OPTFLAG_START_FRAME:  /* T */
        status = sixel_encoder_apply_start_frame_option(encoder, value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_PALETTE_TYPE:  /* t */
        status = sixel_encoder_parse_choice_argument(
            value,
            g_option_choices_palette_type,
            sizeof(g_option_choices_palette_type) /
            sizeof(g_option_choices_palette_type[0]),
            "cannot parse palette type option.",
            &match_value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        encoder->palette_type = match_value;
        break;
    case SIXEL_OPTFLAG_BGCOLOR:  /* B */
        /* parse --bgcolor option */
        if (encoder->bgcolor) {
            sixel_allocator_free(encoder->allocator, encoder->bgcolor);
            encoder->bgcolor = NULL;
        }
        status = sixel_parse_x_colorspec(&encoder->bgcolor,
                                         value,
                                         encoder->allocator);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "cannot parse bgcolor option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->bgcolor_source = SIXEL_LOADER_BGCOLOR_SOURCE_EXPLICIT;
        break;
    case SIXEL_OPTFLAG_INSECURE:  /* k */
        encoder->finsecure = 1;
        break;
    case SIXEL_OPTFLAG_INVERT:  /* i */
        encoder->finvert = 1;
        break;
    case SIXEL_OPTFLAG_USE_MACRO:  /* u */
        encoder->fuse_macro = 1;
        break;
    case SIXEL_OPTFLAG_MACRO_NUMBER:  /* n */
        status = sixel_encoder_apply_macro_number_option(encoder, value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_IGNORE_DELAY:  /* g */
        encoder->fignore_delay = 1;
        break;
    case SIXEL_OPTFLAG_VERBOSE:  /* v */
        encoder->verbose = 1;
        break;
    case SIXEL_OPTFLAG_LOADERS:  /* L */
        if (encoder->loader_order != NULL) {
            sixel_allocator_free(encoder->allocator,
                                 encoder->loader_order);
            encoder->loader_order = NULL;
        }
        status = sixel_encoder_parse_loader_order(encoder->allocator,
                                                  value,
                                                  &encoder->loader_order);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_STATIC:  /* S */
        encoder->fstatic = 1;
        break;
    case SIXEL_OPTFLAG_DRCS:  /* @ */
        status = sixel_encoder_apply_drcs_option(encoder, value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_PENETRATE:  /* P */
        encoder->penetrate_multiplexer = 1;
        break;
    case SIXEL_OPTFLAG_ENCODE_POLICY:  /* E */
        status = sixel_encoder_parse_choice_argument(
            value,
            g_option_choices_encode_policy,
            sizeof(g_option_choices_encode_policy) /
            sizeof(g_option_choices_encode_policy[0]),
            "cannot parse encode policy option.",
            &match_value);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        encoder->encode_policy = match_value;
        break;
    case SIXEL_OPTFLAG_LUT_POLICY:  /* ~ */
        status = sixel_encoder_parse_lut_policy_argument(
            value,
            &match_value,
            &lut_shared_instance_override,
            &lut_shared_instance_value,
            match_detail,
            sizeof(match_detail));
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        encoder->lut_policy = match_value;
        encoder->lut_policy_override = 1;
        encoder->lut_policy_shared_instance_override =
            lut_shared_instance_override;
        encoder->lut_policy_shared_instance = lut_shared_instance_value;
        if (encoder->dither_cache != NULL) {
            sixel_dither_set_lut_policy(encoder->dither_cache,
                                        encoder->lut_policy);
            ((sixel_dither_t *)encoder->dither_cache)
                ->lut_policy_shared_instance_override
                = encoder->lut_policy_shared_instance_override;
            ((sixel_dither_t *)encoder->dither_cache)
                ->lut_policy_shared_instance
                = encoder->lut_policy_shared_instance;
        }
        break;
    case SIXEL_OPTFLAG_CLUSTERING_COLORSPACE:  /* X */
        if (value == NULL) {
            sixel_helper_set_additional_message(
                "clustering-colorspace requires an argument.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        } else {
            len = strlen(value);

            if (len >= sizeof(lowered)) {
                sixel_helper_set_additional_message(
                    "specified clustering colorspace name is too long.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            for (i = 0; i < len; ++i) {
                lowered[i] = (char)tolower((unsigned char)value[i]);
            }
            lowered[len] = '\0';

            status = sixel_encoder_parse_choice_argument(
                lowered,
                g_option_choices_working_colorspace,
                sizeof(g_option_choices_working_colorspace) /
                sizeof(g_option_choices_working_colorspace[0]),
                "unsupported clustering colorspace specified.",
                &match_value);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            encoder->clustering_colorspace = match_value;
            encoder->clustering_colorspace_set = 1;
            /*
             * Clustering space only controls palette construction.  Do not
             * promote the main image path here; fixed palettes such as -b,
             * -m, and -e have no clustering step, so -X must not change their
             * emitted pixels or palette interpretation.
             */
        }
        break;
    case SIXEL_OPTFLAG_WORKING_COLORSPACE:  /* W */
        if (value == NULL) {
            sixel_helper_set_additional_message(
                "working-colorspace requires an argument.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        } else {
            len = strlen(value);

            if (len >= sizeof(lowered)) {
                sixel_helper_set_additional_message(
                    "specified working colorspace name is too long.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            for (i = 0; i < len; ++i) {
                lowered[i] = (char)tolower((unsigned char)value[i]);
            }
            lowered[len] = '\0';

            status = sixel_encoder_parse_choice_argument(
                lowered,
                g_option_choices_working_colorspace,
                sizeof(g_option_choices_working_colorspace) /
                sizeof(g_option_choices_working_colorspace[0]),
                "unsupported working colorspace specified.",
                &match_value);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            encoder->working_colorspace = match_value;
            encoder->working_colorspace_set = 1;
            if (encoder->clustering_colorspace_set == 0) {
                encoder->clustering_colorspace = match_value;
            }
            encoder->force_float32_colorspace = 1;
            encoder->prefer_float32 = 1;
        }
        break;
    case SIXEL_OPTFLAG_OUTPUT_COLORSPACE:  /* U */
        if (value == NULL) {
            sixel_helper_set_additional_message(
                "output-colorspace requires an argument.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        } else {
            len = strlen(value);

            if (len >= sizeof(lowered)) {
                sixel_helper_set_additional_message(
                    "specified output colorspace name is too long.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            for (i = 0; i < len; ++i) {
                lowered[i] = (char)tolower((unsigned char)value[i]);
            }
            lowered[len] = '\0';

            status = sixel_encoder_parse_choice_argument(
                lowered,
                g_option_choices_output_colorspace,
                sizeof(g_option_choices_output_colorspace) /
                sizeof(g_option_choices_output_colorspace[0]),
                "unsupported output colorspace specified.",
                &match_value);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            encoder->output_colorspace = match_value;
        }
        break;
    case SIXEL_OPTFLAG_ORMODE:  /* O */
        encoder->ormode = 1;
        break;
    case SIXEL_OPTFLAG_COMPLEXION_SCORE:  /* C */
        (void)value;  /* deprecated no-op option */
        break;
    case SIXEL_OPTFLAG_PIPE_MODE:  /* D */
        encoder->pipe_mode = 1;
        break;
    case '?':  /* unknown option */
    default:
        /* exit if unknown options are specified */
        sixel_helper_set_additional_message(
            "unknown option is specified.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    /* detects arguments conflictions */
    if (encoder->reqcolors != (-1)) {
        switch (encoder->color_option) {
        case SIXEL_COLOR_OPTION_MAPFILE:
            sixel_helper_set_additional_message(
                "option -p, --colors conflicts with -m, --mapfile.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        case SIXEL_COLOR_OPTION_MONOCHROME:
            sixel_helper_set_additional_message(
                "option -e, --monochrome conflicts with -p, --colors.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        case SIXEL_COLOR_OPTION_HIGHCOLOR:
            sixel_helper_set_additional_message(
                "option -p, --colors conflicts with -I, --high-color.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        case SIXEL_COLOR_OPTION_BUILTIN:
            sixel_helper_set_additional_message(
                "option -p, --colors conflicts with -b, --builtin-palette.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        default:
            break;
        }
    }

    /* 8bit output option(-8) conflicts width GNU Screen integration(-P) */
    if (encoder->f8bit && encoder->penetrate_multiplexer) {
        sixel_helper_set_additional_message(
            "option -8 --8bit-mode conflicts"
            " with -P, --penetrate.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_encoder_setopt_context_dispose(&setopt_context);
    sixel_encoder_unref(encoder);

    return status;
}


static void
sixel_encoder_frame_pipeline_request_stop_locked(
    sixel_encoder_frame_pipeline_t *pipeline,
    SIXELSTATUS status,
    sixel_encoder_handoff_trace_reason_t reason)
{
    SIXELSTATUS effective_status;

    effective_status = status;

    if (pipeline == NULL) {
        return;
    }

    if (pipeline->worker_status == SIXEL_OK && status != SIXEL_OK) {
        pipeline->worker_status = status;
    }
    if (pipeline->worker_status != SIXEL_OK) {
        effective_status = pipeline->worker_status;
    }
    pipeline->loader_done = 1;
    sixel_cond_broadcast(&pipeline->cond);

    sixel_encoder_handoff_trace_emit(
        pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_PIPELINE_STOP,
        -1,
        -1,
        effective_status,
        reason);

    return;
}


static void
sixel_encoder_frame_pipeline_request_stop(
    sixel_encoder_frame_pipeline_t *pipeline,
    SIXELSTATUS status,
    sixel_encoder_handoff_trace_reason_t reason)
{
    if (pipeline == NULL || pipeline->initialized == 0) {
        return;
    }

    sixel_mutex_lock(&pipeline->mutex);
    sixel_encoder_frame_pipeline_request_stop_locked(pipeline,
                                                     status,
                                                     reason);
    sixel_mutex_unlock(&pipeline->mutex);
}


static int
sixel_encoder_frame_pipeline_worker(void *priv)
{
    sixel_encoder_frame_pipeline_t *pipeline;
    sixel_frame_t *frame;
    sixel_frame_t *work_frame;
    sixel_encoder_frame_handoff_meta_t metadata;
    SIXELSTATUS status;
    int need_clone;

    pipeline = (sixel_encoder_frame_pipeline_t *)priv;
    frame = NULL;
    work_frame = NULL;
    metadata.frame_no = 0;
    metadata.loop_no = 0;
    metadata.delay = 0;
    metadata.multiframe = 0;
    metadata.needs_preplan_clone = 0;
    status = SIXEL_OK;
    need_clone = 0;

    if (pipeline == NULL || pipeline->encoder == NULL) {
        return 0;
    }

    sixel_encoder_handoff_trace_emit(
        pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_START,
        -1,
        -1,
        SIXEL_OK,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);

    for (;;) {
        sixel_mutex_lock(&pipeline->mutex);
        while (pipeline->queue_count == 0
               && pipeline->loader_done == 0
               && pipeline->worker_status == SIXEL_OK) {
            sixel_encoder_handoff_trace_emit(
                pipeline,
                SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_WAIT,
                -1,
                -1,
                pipeline->worker_status,
                SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
            sixel_cond_wait(&pipeline->cond, &pipeline->mutex);
        }
        if (pipeline->worker_status != SIXEL_OK) {
            sixel_encoder_handoff_trace_emit(
                pipeline,
                SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_BREAK,
                -1,
                -1,
                pipeline->worker_status,
                SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
            sixel_mutex_unlock(&pipeline->mutex);
            break;
        }
        if (pipeline->queue_count == 0) {
            sixel_encoder_handoff_trace_emit(
                pipeline,
                SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_BREAK,
                -1,
                -1,
                pipeline->worker_status,
                SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
            sixel_mutex_unlock(&pipeline->mutex);
            break;
        }
        frame = pipeline->queue[pipeline->queue_head];
        pipeline->queue[pipeline->queue_head] = NULL;
        metadata = pipeline->queue_meta[pipeline->queue_head];
        memset(&pipeline->queue_meta[pipeline->queue_head],
               0,
               sizeof(pipeline->queue_meta[pipeline->queue_head]));
        pipeline->queue_head = (pipeline->queue_head + 1)
            % SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY;
        --pipeline->queue_count;
        sixel_cond_broadcast(&pipeline->cond);
        sixel_mutex_unlock(&pipeline->mutex);

        sixel_encoder_handoff_trace_emit(
            pipeline,
            SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_POP,
            metadata.frame_no,
            metadata.loop_no,
            SIXEL_OK,
            SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
        need_clone = 0;
        work_frame = frame;
        if (sixel_encoder_frame_get_handoff_shareable(frame) != 0 &&
            metadata.needs_preplan_clone != 0) {
            status = sixel_encoder_frame_clone(
                frame,
                pipeline->encoder->allocator,
                &work_frame);
            if (SIXEL_FAILED(status)) {
                sixel_frame_unref(frame);
                frame = NULL;
                sixel_encoder_frame_pipeline_request_stop(
                    pipeline,
                    status,
                    SIXEL_ENCODER_HANDOFF_TRACE_REASON_WORKER_CLONE_FAILED);
                break;
            }
            need_clone = 1;
            sixel_encoder_handoff_trace_emit(
                pipeline,
                SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_CLONE_FALLBACK_ENABLED,
                metadata.frame_no,
                metadata.loop_no,
                SIXEL_OK,
                SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
        }
        status = sixel_encoder_encode_frame(pipeline->encoder,
                                            work_frame,
                                            pipeline->output,
                                            &metadata);
        if (need_clone != 0) {
            sixel_frame_unref(work_frame);
            work_frame = NULL;
        }
        sixel_frame_unref(frame);
        frame = NULL;
        sixel_encoder_handoff_trace_emit(
            pipeline,
            SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_ENCODED,
            metadata.frame_no,
            metadata.loop_no,
            status,
            SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
        if (status != SIXEL_OK) {
            sixel_encoder_frame_pipeline_request_stop(
                pipeline,
                status,
                SIXEL_ENCODER_HANDOFF_TRACE_REASON_WORKER_ERROR);
            break;
        }
    }

    sixel_encoder_handoff_trace_emit(
        pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_WORKER_EXIT,
        -1,
        -1,
        status,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
    return 0;
}


static SIXELSTATUS
sixel_encoder_frame_pipeline_init(sixel_encoder_frame_pipeline_t *pipeline,
                                  sixel_encoder_t *encoder,
                                  sixel_output_t *output)
{
    SIXELSTATUS status;
    int i;
    int result;

    status = SIXEL_OK;
    i = 0;
    result = 0;

    if (pipeline == NULL || encoder == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (i = 0; i < SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY; ++i) {
        pipeline->queue[i] = NULL;
        memset(&pipeline->queue_meta[i], 0, sizeof(pipeline->queue_meta[i]));
    }
    pipeline->encoder = encoder;
    pipeline->output = output;
    pipeline->worker_status = SIXEL_OK;
    pipeline->queue_head = 0;
    pipeline->queue_tail = 0;
    pipeline->queue_count = 0;
    pipeline->initialized = 0;
    pipeline->started = 0;
    pipeline->loader_done = 0;
    pipeline->handoff_mode = SIXEL_ENCODER_HANDOFF_UNDECIDED;

#if !SIXEL_ENABLE_THREADS
    pipeline->handoff_mode = SIXEL_ENCODER_HANDOFF_SERIAL;
    return SIXEL_OK;
#endif  /* !SIXEL_ENABLE_THREADS */

    result = sixel_mutex_init(&pipeline->mutex);
    if (result != 0) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    result = sixel_cond_init(&pipeline->cond);
    if (result != 0) {
        sixel_mutex_destroy(&pipeline->mutex);
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    pipeline->initialized = 1;

end:
    return status;
}


static void
sixel_encoder_frame_pipeline_dispose(sixel_encoder_frame_pipeline_t *pipeline)
{
    int i;

    i = 0;

    if (pipeline == NULL || pipeline->initialized == 0) {
        return;
    }

    for (i = 0; i < SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY; ++i) {
        if (pipeline->queue[i] != NULL) {
            sixel_frame_unref(pipeline->queue[i]);
            pipeline->queue[i] = NULL;
        }
        memset(&pipeline->queue_meta[i], 0, sizeof(pipeline->queue_meta[i]));
    }

    sixel_cond_destroy(&pipeline->cond);
    sixel_mutex_destroy(&pipeline->mutex);
    pipeline->initialized = 0;
    pipeline->started = 0;
    pipeline->handoff_mode = SIXEL_ENCODER_HANDOFF_SERIAL;
}


static SIXELSTATUS
sixel_encoder_frame_pipeline_enqueue(sixel_encoder_frame_pipeline_t *pipeline,
                                     sixel_frame_t *frame)
{
    SIXELSTATUS status;
    sixel_frame_t *queue_frame;
    sixel_encoder_frame_handoff_meta_t metadata;
    int cancel_requested;

    status = SIXEL_OK;
    queue_frame = NULL;
    metadata.frame_no = 0;
    metadata.loop_no = 0;
    metadata.delay = 0;
    metadata.multiframe = 0;
    metadata.needs_preplan_clone = 0;
    cancel_requested = 0;

    if (pipeline == NULL || frame == NULL || pipeline->initialized == 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    metadata.frame_no = sixel_frame_get_frame_no(frame);
    metadata.loop_no = sixel_frame_get_loop_no(frame);
    metadata.delay = sixel_frame_get_delay(frame);
    metadata.multiframe = sixel_frame_get_multiframe(frame);

    if (sixel_encoder_frame_get_handoff_shareable(frame) != 0) {
        metadata.needs_preplan_clone =
            sixel_encoder_handoff_needs_preplan_clone(pipeline->encoder,
                                                      frame);
        queue_frame = frame;
        sixel_frame_ref(queue_frame);
        sixel_encoder_handoff_trace_emit(
            pipeline,
            SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_BY_REF,
            metadata.frame_no,
            metadata.loop_no,
            SIXEL_OK,
            SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
    } else {
        status = sixel_encoder_frame_clone(frame,
                                           pipeline->encoder->allocator,
                                           &queue_frame);
        if (SIXEL_FAILED(status)) {
            sixel_encoder_handoff_trace_emit(
                pipeline,
                SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_CLONE_FAILED,
                metadata.frame_no,
                metadata.loop_no,
                status,
                SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
            return status;
        }
    }

    if (sixel_encoder_is_cancel_requested(pipeline->encoder) != 0) {
        status = SIXEL_INTERRUPTED;
        sixel_encoder_frame_pipeline_request_stop(
            pipeline,
            status,
            SIXEL_ENCODER_HANDOFF_TRACE_REASON_ENQUEUE_CANCEL_BEFORE_LOCK);
        sixel_frame_unref(queue_frame);
        return status;
    }

    sixel_mutex_lock(&pipeline->mutex);
    if (sixel_encoder_is_cancel_requested(pipeline->encoder) != 0) {
        cancel_requested = 1;
    }
    if (cancel_requested != 0) {
        sixel_encoder_frame_pipeline_request_stop_locked(
            pipeline,
            SIXEL_INTERRUPTED,
            SIXEL_ENCODER_HANDOFF_TRACE_REASON_ENQUEUE_CANCEL_PREWAIT);
    }
    while (pipeline->queue_count >= SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY
           && pipeline->worker_status == SIXEL_OK) {
        sixel_encoder_handoff_trace_emit(
            pipeline,
            SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_WAIT,
            metadata.frame_no,
            metadata.loop_no,
            pipeline->worker_status,
            SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
        sixel_cond_wait(&pipeline->cond, &pipeline->mutex);
        if (sixel_encoder_is_cancel_requested(pipeline->encoder) != 0) {
            sixel_encoder_frame_pipeline_request_stop_locked(
                pipeline,
                SIXEL_INTERRUPTED,
                SIXEL_ENCODER_HANDOFF_TRACE_REASON_ENQUEUE_CANCEL_WAIT);
            break;
        }
    }
    if (pipeline->worker_status != SIXEL_OK) {
        status = pipeline->worker_status;
        sixel_mutex_unlock(&pipeline->mutex);
        sixel_frame_unref(queue_frame);
        sixel_encoder_handoff_trace_emit(
            pipeline,
            SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_ABORT,
            metadata.frame_no,
            metadata.loop_no,
            status,
            SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
        return status;
    }

    pipeline->queue[pipeline->queue_tail] = queue_frame;
    pipeline->queue_meta[pipeline->queue_tail] = metadata;
    pipeline->queue_tail = (pipeline->queue_tail + 1)
        % SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY;
    ++pipeline->queue_count;
    sixel_cond_signal(&pipeline->cond);
    sixel_mutex_unlock(&pipeline->mutex);
    sixel_encoder_handoff_trace_emit(
        pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENQUEUE_OK,
        metadata.frame_no,
        metadata.loop_no,
        SIXEL_OK,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);

    return status;
}


static SIXELSTATUS
sixel_encoder_frame_pipeline_finish(sixel_encoder_frame_pipeline_t *pipeline)
{
    SIXELSTATUS status;
    int started;
    int queue_count;
    int loader_done;
    SIXELSTATUS worker_status;
    int cancel_requested;

    status = SIXEL_OK;
    started = 0;
    queue_count = 0;
    loader_done = 0;
    worker_status = SIXEL_OK;
    cancel_requested = 0;

    if (pipeline == NULL || pipeline->initialized == 0) {
        return SIXEL_OK;
    }

    sixel_mutex_lock(&pipeline->mutex);
    started = pipeline->started;
    queue_count = pipeline->queue_count;
    loader_done = pipeline->loader_done;
    worker_status = pipeline->worker_status;
    if (sixel_encoder_is_cancel_requested(pipeline->encoder) != 0
        && pipeline->worker_status == SIXEL_OK) {
        cancel_requested = 1;
        sixel_encoder_frame_pipeline_request_stop_locked(
            pipeline,
            SIXEL_INTERRUPTED,
            SIXEL_ENCODER_HANDOFF_TRACE_REASON_FINISH_CANCEL);
    } else {
        pipeline->loader_done = 1;
        sixel_cond_broadcast(&pipeline->cond);
    }
    loader_done = pipeline->loader_done;
    worker_status = pipeline->worker_status;
    sixel_mutex_unlock(&pipeline->mutex);
    (void)started;
    (void)queue_count;
    (void)loader_done;
    (void)cancel_requested;
    sixel_encoder_handoff_trace_emit(
        pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_FINISH_BEGIN,
        -1,
        -1,
        worker_status,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);

    if (started != 0) {
        sixel_encoder_handoff_trace_emit(
            pipeline,
            SIXEL_ENCODER_HANDOFF_TRACE_EVENT_FINISH_JOIN_WAIT,
            -1,
            -1,
            SIXEL_OK,
            SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
        sixel_thread_join(&pipeline->thread);
        pipeline->started = 0;
        sixel_encoder_handoff_trace_emit(
            pipeline,
            SIXEL_ENCODER_HANDOFF_TRACE_EVENT_FINISH_JOIN_DONE,
            -1,
            -1,
            SIXEL_OK,
            SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
    }

    sixel_mutex_lock(&pipeline->mutex);
    status = pipeline->worker_status;
    sixel_mutex_unlock(&pipeline->mutex);
    sixel_encoder_handoff_trace_emit(
        pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_FINISH_END,
        -1,
        -1,
        status,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);

    return status;
}


/* called when image loader component load a image frame */
static SIXELSTATUS
sixel_encoder_load_callback_resolve_handoff(
    sixel_encoder_frame_pipeline_t *pipeline,
    sixel_encoding_planner_t *planner,
    sixel_encoder_t *encoder,
    sixel_frame_t *frame)
{
    SIXELSTATUS status;
    int allow_loader_pipeline;
    int quantize_animation_enabled;
    int result;
    int frame_no;
    int loop_no;

    status = SIXEL_OK;
    allow_loader_pipeline = 0;
    quantize_animation_enabled = 0;
    result = 0;
    frame_no = 0;
    loop_no = 0;

    if (pipeline == NULL || encoder == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (pipeline->handoff_mode != SIXEL_ENCODER_HANDOFF_UNDECIDED) {
        return SIXEL_OK;
    }

    frame_no = sixel_frame_get_frame_no(frame);
    loop_no = sixel_frame_get_loop_no(frame);
    if (planner != NULL) {
        sixel_encoding_planner_reset_for_frame(planner);
        sixel_encoding_planner_plan(planner, encoder, frame);
        allow_loader_pipeline = sixel_encoding_planner_update_loader_handoff(
            planner,
            encoder,
            frame);
        quantize_animation_enabled =
            sixel_encoder_quantize_animation_enabled_for_frame(
                encoder,
                frame);
        if (quantize_animation_enabled != 0) {
            /*
             * Quantize animation mode keeps frame history in encoder state.
             * Serialize frame handoff to preserve deterministic order.
             */
            allow_loader_pipeline = 0;
        }
        sixel_encoder_handoff_trace_emit(
            pipeline,
            SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_PLANNER,
            frame_no,
            loop_no,
            SIXEL_OK,
            allow_loader_pipeline != 0
                ? SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE
                : SIXEL_ENCODER_HANDOFF_TRACE_REASON_PLANNER_DISALLOW);
    }

    if (allow_loader_pipeline != 0) {
        pipeline->handoff_mode = SIXEL_ENCODER_HANDOFF_PIPELINE;
        result = sixel_thread_create(&pipeline->thread,
                                     sixel_encoder_frame_pipeline_worker,
                                     pipeline);
        if (result == 0) {
            pipeline->started = 1;
            status = SIXEL_OK;
        } else {
            pipeline->handoff_mode = SIXEL_ENCODER_HANDOFF_SERIAL;
            status = SIXEL_RUNTIME_ERROR;
        }
    } else {
        pipeline->handoff_mode = SIXEL_ENCODER_HANDOFF_SERIAL;
        status = SIXEL_OK;
    }

    sixel_encoder_handoff_trace_emit(
        pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_HANDOFF_DECIDE,
        frame_no,
        loop_no,
        status,
        result == 0
            ? (allow_loader_pipeline != 0
                ? SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE
                : SIXEL_ENCODER_HANDOFF_TRACE_REASON_PLANNER_DISALLOW)
            : SIXEL_ENCODER_HANDOFF_TRACE_REASON_THREAD_CREATE_FAILED);

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_load_callback_dispatch(
    sixel_encoder_load_context_t *context,
    sixel_frame_t *frame)
{
    sixel_encoder_frame_pipeline_t *pipeline;
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    int frame_no;
    int loop_no;

    pipeline = NULL;
    encoder = NULL;
    status = SIXEL_OK;
    frame_no = 0;
    loop_no = 0;

    if (context == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    pipeline = &context->frame_pipeline;
    encoder = context->encoder;
    if (encoder == NULL || pipeline == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame_no = sixel_frame_get_frame_no(frame);
    loop_no = sixel_frame_get_loop_no(frame);
    if (pipeline->handoff_mode == SIXEL_ENCODER_HANDOFF_PIPELINE) {
        sixel_encoder_handoff_trace_emit(
            pipeline,
            SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_ENQUEUE_REQUEST,
            frame_no,
            loop_no,
            SIXEL_OK,
            SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
        status = sixel_encoder_frame_pipeline_enqueue(pipeline, frame);
        sixel_encoder_handoff_trace_emit(
            pipeline,
            SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_ENQUEUE_RESULT,
            frame_no,
            loop_no,
            status,
            SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
        return status;
    }

    sixel_encoder_handoff_trace_emit(
        pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_SERIAL_START,
        frame_no,
        loop_no,
        SIXEL_OK,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
    status = sixel_encoder_encode_frame(encoder, frame, context->output, NULL);
    sixel_encoder_handoff_trace_emit(
        pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_SERIAL_RESULT,
        frame_no,
        loop_no,
        status,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);

    return status;
}

/* called when image loader component load a image frame */
static SIXELSTATUS
load_image_callback(sixel_frame_t *frame, void *data)
{
    sixel_encoder_load_context_t *context;
    sixel_encoder_frame_pipeline_t *pipeline;
    sixel_encoding_planner_t *planner;
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    int frame_no;
    int loop_no;

    context = (sixel_encoder_load_context_t *)data;
    pipeline = NULL;
    planner = NULL;
    encoder = NULL;
    status = SIXEL_OK;
    frame_no = 0;
    loop_no = 0;

    if (context == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    encoder = context->encoder;
    planner = context->planner;
    pipeline = &context->frame_pipeline;
    if (encoder == NULL || pipeline == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame_no = sixel_frame_get_frame_no(frame);
    loop_no = sixel_frame_get_loop_no(frame);
    sixel_encoder_handoff_trace_emit(
        pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_ENTER,
        frame_no,
        loop_no,
        SIXEL_OK,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);

    if (sixel_encoder_is_cancel_requested(encoder) != 0) {
        if (pipeline->handoff_mode == SIXEL_ENCODER_HANDOFF_PIPELINE) {
            sixel_encoder_frame_pipeline_request_stop(
                pipeline,
                SIXEL_INTERRUPTED,
                SIXEL_ENCODER_HANDOFF_TRACE_REASON_CALLBACK_CANCEL);
        }
        return SIXEL_INTERRUPTED;
    }

    if (context->psd_trace_only != 0) {
        /*
         * PSD trace-only mode keeps loader-side diagnostics but skips frame
         * encoding and output generation to reduce per-test runtime.
         */
        return SIXEL_OK;
    }

    status = sixel_encoder_load_callback_resolve_handoff(pipeline,
                                                          planner,
                                                          encoder,
                                                          frame);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    /*
     * Emit a stable post-handoff marker before dispatching into either
     * pipeline enqueue or serial encode paths. Tests use this token to send
     * SIGINT after handoff selection regardless of thread configuration.
     * The test runner can also opt into a direct SIGUSR1 synchronization
     * signal here so trigger delivery stays deterministic across runtimes.
     * When the runner exports sync metadata, wait for cancel so tiny inputs
     * cannot finish before the injected SIGINT reaches the callback path.
     */
    sixel_encoder_test_sigint_sync_notify_dispatch_start();
    sixel_encoder_handoff_trace_emit(
        pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_CALLBACK_DISPATCH_START,
        frame_no,
        loop_no,
        SIXEL_OK,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
    sixel_encoder_test_sigint_sync_wait_for_cancel(encoder);

    return sixel_encoder_load_callback_dispatch(context, frame);
}

static int
temp_debug_enabled(void)
{
    char const *flag;

    flag = sixel_compat_getenv("_SIXEL_DEBUG_TEMP");
    if (flag == NULL || flag[0] == '\0') {
        return 0;
    }
    if (flag[0] == '0' && flag[1] == '\0') {
        return 0;
    }

    return 1;
}

static void
temp_debug_log(char const *stage,
               char const *path,
               int error_number)
{
    char error_buffer[128];
    char const *error_message;

    if (!temp_debug_enabled()) {
        return;
    }

    error_message = "";
    error_buffer[0] = '\0';
    if (error_number != 0) {
        error_message = sixel_compat_strerror(error_number,
                                              error_buffer,
                                              sizeof(error_buffer));
        if (error_message == NULL) {
            error_message = "(unknown)";
        }
    }

    (void)fprintf(stderr,
                  "debug(temp): stage=%s path=%s errno=%d message=%s\n",
                  stage != NULL ? stage : "(null)",
                  path != NULL ? path : "(null)",
                  error_number,
                  error_number != 0 ? error_message : "");
}

static void
temp_debug_log_note(char const *stage,
                    char const *note)
{
    if (!temp_debug_enabled()) {
        return;
    }

    (void)fprintf(stderr,
                  "debug(temp): stage=%s note=%s\n",
                  stage != NULL ? stage : "(null)",
                  note != NULL ? note : "(null)");
}

static int
temp_directory_is_writable(char const *tmpdir)
{
    struct stat temp_stat;
    int stat_result;
    int saved_errno;
    char note_buffer[96];
    int note_size;
    int access_mode;
    int access_result;

    if (tmpdir == NULL || tmpdir[0] == '\0') {
        temp_debug_log("tmpdir_empty", tmpdir, 0);
        return 0;
    }
    temp_debug_log("tmpdir_probe", tmpdir, 0);

    stat_result = sixel_compat_stat(tmpdir, &temp_stat);
    if (stat_result != 0 || !S_ISDIR(temp_stat.st_mode)) {
        saved_errno = errno;
        temp_debug_log("tmpdir_stat_fail", tmpdir, saved_errno);
        return 0;
    }
    note_size = sixel_compat_snprintf(note_buffer,
                                      sizeof(note_buffer),
                                      "stat_mode=0%o",
                                      (unsigned int)temp_stat.st_mode);
    if (note_size > 0 && (size_t)note_size < sizeof(note_buffer)) {
        temp_debug_log_note("tmpdir_stat_ok", note_buffer);
    }

#if defined(_WIN32)
    /*
     * MSVC's _access(..., W_OK) can report false negatives for writable
     * directories. For temp probing on Windows, existence is enough because
     * the subsequent open() path is the authoritative write check.
     */
    access_mode = F_OK;
#elif defined(W_OK)
    access_mode = W_OK;
#else
    access_mode = F_OK;
#endif
    access_result = sixel_compat_access(tmpdir, access_mode);
    if (access_result != 0) {
        saved_errno = errno;
        temp_debug_log("tmpdir_access_fail", tmpdir, saved_errno);
        return 0;
    }
    note_size = sixel_compat_snprintf(note_buffer,
                                      sizeof(note_buffer),
                                      "access_mode=%d",
                                      access_mode);
    if (note_size > 0 && (size_t)note_size < sizeof(note_buffer)) {
        temp_debug_log_note("tmpdir_access_mode", note_buffer);
    }

    temp_debug_log("tmpdir_access_ok", tmpdir, 0);
    return 1;
}

static char *
create_temp_template_with_prefix(sixel_allocator_t *allocator,
                                 char const *prefix,
                                 size_t *capacity_out)
{
    char const *tmpdir;
    size_t tmpdir_len;
    size_t prefix_len;
    size_t suffix_len;
    size_t template_len;
    char *template_path;
    int needs_separator;
    size_t maximum_tmpdir_len;
    char separator;
    int tmpdir_writable;
    char note_buffer[128];
    int note_size;
#if defined(__EMSCRIPTEN__)
    char pid_buffer[32];
    int pid_chars;
    size_t pid_len;
#endif

#if defined(_WIN32)
    /*
     * Wine test runners export host-style TMPDIR values such as
     * "/home/runner/...". MinGW mkstemp() may reject these with
     * EINVAL, so prefer native Windows temp variables first.
     */
    tmpdir = sixel_compat_getenv("TEMP");
    temp_debug_log("tmpdir_env_temp", tmpdir, 0);
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        temp_debug_log_note("tmpdir_env_temp_empty", "trying TMP");
        tmpdir = sixel_compat_getenv("TMP");
        temp_debug_log("tmpdir_env_tmp", tmpdir, 0);
    }
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        temp_debug_log_note("tmpdir_env_tmp_empty", "trying TMPDIR");
        tmpdir = sixel_compat_getenv("TMPDIR");
        temp_debug_log("tmpdir_env_tmpdir", tmpdir, 0);
    }
#else
    tmpdir = sixel_compat_getenv("TMPDIR");
    temp_debug_log("tmpdir_env_tmpdir", tmpdir, 0);
#endif
    if (tmpdir == NULL || tmpdir[0] == '\0') {
#if defined(_WIN32)
        tmpdir = ".";
#else
        tmpdir = "/tmp";
#endif
        temp_debug_log("tmpdir_defaulted", tmpdir, 0);
    }
    tmpdir_writable = temp_directory_is_writable(tmpdir);
    if (!tmpdir_writable) {
        temp_debug_log("tmpdir_primary_rejected", tmpdir, 0);
#if !defined(_WIN32)
        if (temp_directory_is_writable("/tmp")) {
            temp_debug_log_note("tmpdir_fallback_selected", "using /tmp");
            tmpdir = "/tmp";
        } else
#endif
        {
            temp_debug_log_note("tmpdir_fallback_selected", "using .");
            tmpdir = ".";
        }
    } else {
        temp_debug_log_note("tmpdir_primary_selected", "primary accepted");
    }
    temp_debug_log("tmpdir_selected", tmpdir, 0);

    tmpdir_len = strlen(tmpdir);
    prefix_len = 0u;
    suffix_len = 0u;
    if (prefix == NULL) {
        temp_debug_log_note("template_prefix_null", "prefix is null");
        return NULL;
    }

    prefix_len = strlen(prefix);
#if defined(__EMSCRIPTEN__)
    /*
     * Emscripten NODERAWFS environments can return the same mktemp() result
     * across parallel processes. Include the process id in the template
     * prefix so concurrent workers do not race on the same staging path.
     */
    pid_chars = sixel_compat_snprintf(pid_buffer,
                                      sizeof(pid_buffer),
                                      "%lu",
                                      (unsigned long)getpid());
    if (pid_chars <= 0 || (size_t)pid_chars >= sizeof(pid_buffer)) {
        temp_debug_log_note("template_pid_format_failed",
                            "failed to format pid component");
        return NULL;
    }
    pid_len = (size_t)pid_chars;
    suffix_len = prefix_len + 1u + pid_len + strlen("-XXXXXX");
#else
    suffix_len = prefix_len + strlen("-XXXXXX");
#endif
    maximum_tmpdir_len = (size_t)INT_MAX;
    note_size = sixel_compat_snprintf(
        note_buffer,
        sizeof(note_buffer),
        "tmpdir_len=%lu prefix_len=%lu suffix_len=%lu",
        (unsigned long)tmpdir_len,
        (unsigned long)prefix_len,
        (unsigned long)suffix_len);
    if (note_size > 0 && (size_t)note_size < sizeof(note_buffer)) {
        temp_debug_log_note("template_length_inputs", note_buffer);
    }

    if (maximum_tmpdir_len <= suffix_len + 2) {
        temp_debug_log_note("template_length_guard_fail",
                            "INT_MAX bound too small");
        return NULL;
    }
    if (tmpdir_len > maximum_tmpdir_len - (suffix_len + 2)) {
        note_size = sixel_compat_snprintf(note_buffer,
                                          sizeof(note_buffer),
                                          "tmpdir_len=%lu max=%lu",
                                          (unsigned long)tmpdir_len,
                                          (unsigned long)(
                                              maximum_tmpdir_len -
                                              (suffix_len + 2)));
        if (note_size > 0 && (size_t)note_size < sizeof(note_buffer)) {
            temp_debug_log_note("template_length_overflow", note_buffer);
        }
        return NULL;
    }
    separator = '/';
#if defined(_WIN32)
    /*
     * Prefer Windows separators for native roots (for example "." or
     * "C:\\temp"), but keep slash-only roots (for example "/tmp")
     * slash-delimited.
     */
    if (strchr(tmpdir, '\\') != NULL) {
        separator = '\\';
    } else if (strchr(tmpdir, '/') == NULL) {
        separator = '\\';
    }
#endif
    needs_separator = 1;
    if (tmpdir_len > 0) {
        if (tmpdir[tmpdir_len - 1] == '/' || tmpdir[tmpdir_len - 1] == '\\') {
            needs_separator = 0;
        }
    }
    note_size = sixel_compat_snprintf(note_buffer,
                                      sizeof(note_buffer),
                                      "needs_separator=%d separator=%c",
                                      needs_separator,
                                      separator);
    if (note_size > 0 && (size_t)note_size < sizeof(note_buffer)) {
        temp_debug_log_note("template_separator", note_buffer);
    }

    template_len = tmpdir_len + suffix_len + 2;
    note_size = sixel_compat_snprintf(note_buffer,
                                      sizeof(note_buffer),
                                      "template_len=%lu",
                                      (unsigned long)template_len);
    if (note_size > 0 && (size_t)note_size < sizeof(note_buffer)) {
        temp_debug_log_note("template_alloc_request", note_buffer);
    }
    template_path = (char *)sixel_allocator_malloc(allocator, template_len);
    if (template_path == NULL) {
        temp_debug_log("template_alloc_failed", tmpdir, errno);
        return NULL;
    }

    if (needs_separator) {
#if defined(__EMSCRIPTEN__)
        (void) snprintf(template_path, template_len,
                        "%s%c%s-%s-XXXXXX",
                        tmpdir,
                        separator,
                        prefix,
                        pid_buffer);
#else
        (void) snprintf(template_path, template_len,
                        "%s%c%s-XXXXXX", tmpdir, separator, prefix);
#endif
    } else {
#if defined(__EMSCRIPTEN__)
        (void) snprintf(template_path, template_len,
                        "%s%s-%s-XXXXXX",
                        tmpdir,
                        prefix,
                        pid_buffer);
#else
        (void) snprintf(template_path, template_len,
                        "%s%s-XXXXXX", tmpdir, prefix);
#endif
    }
    temp_debug_log("template_built", template_path, 0);

    if (capacity_out != NULL) {
        *capacity_out = template_len;
    }

    return template_path;
}


static char *
create_temp_template(sixel_allocator_t *allocator,
                     size_t *capacity_out)
{
    return create_temp_template_with_prefix(allocator,
                                            "img2sixel",
                                            capacity_out);
}


static void
clipboard_select_format(char *dest,
                        size_t dest_size,
                        char const *format,
                        char const *fallback)
{
    char const *source;
    size_t limit;

    if (dest == NULL || dest_size == 0u) {
        return;
    }

    source = fallback;
    if (format != NULL && format[0] != '\0') {
        source = format;
    }

    limit = dest_size - 1u;
    if (limit == 0u) {
        dest[0] = '\0';
        return;
    }

    (void)snprintf(dest, dest_size, "%.*s", (int)limit, source);
}


static SIXELSTATUS
clipboard_create_spool(sixel_allocator_t *allocator,
                       char const *prefix,
                       char **path_out,
                       int *fd_out)
{
    SIXELSTATUS status;
    char *template_path;
    size_t template_capacity;
    int open_flags;
#if defined(O_EXCL) && !defined(_WIN32)
    int retry_flags;
#endif
    int open_attempt;
    int open_errno;
    int fd;
    char *tmpname_result;

    status = SIXEL_FALSE;
    template_path = NULL;
    template_capacity = 0u;
    open_flags = 0;
#if defined(O_EXCL) && !defined(_WIN32)
    retry_flags = 0;
#endif
    fd = (-1);
    tmpname_result = NULL;

    template_path = create_temp_template_with_prefix(allocator,
                                                     prefix,
                                                     &template_capacity);
    if (template_path == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to allocate spool template.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    temp_debug_log("clipboard_template_allocated", template_path, 0);

    if (sixel_compat_mktemp(template_path, template_capacity) != 0) {
        temp_debug_log("clipboard_mktemp_failed", template_path, errno);
        tmpname_result = sixel_compat_tmpnam(template_path,
                                             template_capacity);
        if (tmpname_result == NULL) {
            sixel_helper_set_additional_message(
                "clipboard: failed to reserve spool template.");
            status = SIXEL_LIBC_ERROR;
            goto end;
        }
        template_capacity = strlen(template_path) + 1u;
        temp_debug_log("clipboard_tmpnam_fallback", template_path, 0);
    }

    open_flags = O_RDWR | O_CREAT | O_TRUNC;
#if defined(O_BINARY)
    open_flags |= O_BINARY;
#endif
#if defined(O_EXCL) && !defined(_WIN32)
    retry_flags = open_flags;
#endif
    /*
     * Windows CRTs can report spurious failures with O_EXCL on temp paths
     * produced by mktemp-style APIs, so keep temp spool creation on
     * O_CREAT|O_TRUNC there.
     */
#if defined(O_EXCL) && !defined(_WIN32)
    open_flags |= O_EXCL;
    retry_flags &= ~O_EXCL;
#endif
    open_attempt = 0;
    open_errno = 0;
    for (open_attempt = 0; open_attempt < 4; ++open_attempt) {
        fd = sixel_compat_open(template_path, open_flags, S_IRUSR | S_IWUSR);
#if defined(O_EXCL) && !defined(_WIN32)
        if (fd < 0
                && (errno == EBADF
                    || errno == EINVAL
                    || errno == ENOENT
                    || errno == EACCES
                    || errno == EPERM)) {
            /*
             * Some Windows CRT configurations reject O_EXCL for paths that are
             * otherwise valid. Retry without O_EXCL before treating it as
             * fatal.
             */
            fd = sixel_compat_open(template_path,
                                   retry_flags,
                                   S_IRUSR | S_IWUSR);
        }
#endif
        if (fd >= 0) {
            break;
        }
        open_errno = errno;
        temp_debug_log("clipboard_open_failed", template_path, open_errno);
        if (open_errno != EEXIST) {
            break;
        }
        /*
         * Emscripten mktemp implementations can return reused names.
         * Regenerate the path and retry when the generated file exists.
         */
        if (sixel_compat_mktemp(template_path, template_capacity) != 0) {
            temp_debug_log("clipboard_mktemp_retry_failed",
                           template_path,
                           errno);
            tmpname_result = sixel_compat_tmpnam(template_path,
                                                 template_capacity);
            if (tmpname_result == NULL) {
                sixel_helper_set_additional_message(
                    "clipboard: failed to reserve spool template.");
                status = SIXEL_LIBC_ERROR;
                goto end;
            }
            template_capacity = strlen(template_path) + 1u;
            temp_debug_log("clipboard_tmpnam_retry", template_path, 0);
        }
    }
    if (fd < 0) {
        if (open_errno != 0) {
            errno = open_errno;
        }
        sixel_helper_set_additional_message(
            "clipboard: failed to open spool file.");
        status = SIXEL_LIBC_ERROR;
        goto end;
    }

    *path_out = template_path;
    if (fd_out != NULL) {
        *fd_out = fd;
        fd = (-1);
    }

    template_path = NULL;
    status = SIXEL_OK;

end:
    if (fd >= 0) {
        (void)sixel_compat_close(fd);
    }
    if (template_path != NULL) {
        sixel_allocator_free(allocator, template_path);
    }

    return status;
}


static SIXELSTATUS
clipboard_write_file(char const *path,
                     unsigned char const *data,
                     size_t size)
{
    FILE *stream;
    size_t written;

    if (path == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: spool path is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    stream = sixel_compat_fopen(path, "wb");
    if (stream == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to open spool file for write.");
        return SIXEL_LIBC_ERROR;
    }

    written = 0u;
    if (size > 0u && data != NULL) {
        written = fwrite(data, 1u, size, stream);
        if (written != size) {
            (void)fclose(stream);
            sixel_helper_set_additional_message(
                "clipboard: failed to write spool payload.");
            return SIXEL_LIBC_ERROR;
        }
    }

    if (fclose(stream) != 0) {
        sixel_helper_set_additional_message(
            "clipboard: failed to close spool file after write.");
        return SIXEL_LIBC_ERROR;
    }

    return SIXEL_OK;
}


static SIXELSTATUS
clipboard_read_file(char const *path,
                    unsigned char **data,
                    size_t *size)
{
    FILE *stream;
    long seek_result;
    long file_size;
    unsigned char *buffer;
    size_t read_size;

    if (data == NULL || size == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: read buffer pointers are null.");
        return SIXEL_BAD_ARGUMENT;
    }

    *data = NULL;
    *size = 0u;

    if (path == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: spool path is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    stream = sixel_compat_fopen(path, "rb");
    if (stream == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to open spool file for read.");
        return SIXEL_LIBC_ERROR;
    }

    seek_result = fseek(stream, 0L, SEEK_END);
    if (seek_result != 0) {
        (void)fclose(stream);
        sixel_helper_set_additional_message(
            "clipboard: failed to seek spool file.");
        return SIXEL_LIBC_ERROR;
    }

    file_size = ftell(stream);
    if (file_size < 0) {
        (void)fclose(stream);
        sixel_helper_set_additional_message(
            "clipboard: failed to determine spool size.");
        return SIXEL_LIBC_ERROR;
    }

    seek_result = fseek(stream, 0L, SEEK_SET);
    if (seek_result != 0) {
        (void)fclose(stream);
        sixel_helper_set_additional_message(
            "clipboard: failed to rewind spool file.");
        return SIXEL_LIBC_ERROR;
    }

    if (file_size == 0) {
        buffer = NULL;
        read_size = 0u;
    } else {
        buffer = (unsigned char *)malloc((size_t)file_size);
        if (buffer == NULL) {
            (void)fclose(stream);
            sixel_helper_set_additional_message(
                "clipboard: malloc() failed for spool payload.");
            return SIXEL_BAD_ALLOCATION;
        }
        read_size = fread(buffer, 1u, (size_t)file_size, stream);
        if (read_size != (size_t)file_size) {
            free(buffer);
            (void)fclose(stream);
            sixel_helper_set_additional_message(
                "clipboard: failed to read spool payload.");
            return SIXEL_LIBC_ERROR;
        }
    }

    if (fclose(stream) != 0) {
        if (buffer != NULL) {
            free(buffer);
        }
        sixel_helper_set_additional_message(
            "clipboard: failed to close spool file after read.");
        return SIXEL_LIBC_ERROR;
    }

    *data = buffer;
    *size = read_size;

    return SIXEL_OK;
}


static SIXELSTATUS
write_png_from_sixel(char const *sixel_path,
                     char const *output_path,
                     sixel_encoder_t *encoder)
{
    SIXELSTATUS status;
    sixel_decoder_t *decoder;

    status = SIXEL_FALSE;
    decoder = NULL;
    sixel_encoder_log_stage(encoder,
                            NULL,
                            "main",
                            "encoder",
                            "png_decode_begin",
                            "input=%s output=%s",
                            sixel_path != NULL ? sixel_path : "(null)",
                            output_path != NULL ? output_path : "(null)");

    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        sixel_encoder_log_stage(encoder,
                                NULL,
                                "main",
                                "encoder",
                                "png_decode_new_failed",
                                "status=%d",
                                status);
        goto end;
    }

    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_INPUT, sixel_path);
    if (SIXEL_FAILED(status)) {
        sixel_encoder_log_stage(encoder,
                                NULL,
                                "main",
                                "encoder",
                                "png_decode_input_setopt_failed",
                                "status=%d",
                                status);
        goto end;
    }

    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_OUTPUT, output_path);
    if (SIXEL_FAILED(status)) {
        sixel_encoder_log_stage(encoder,
                                NULL,
                                "main",
                                "encoder",
                                "png_decode_output_setopt_failed",
                                "status=%d",
                                status);
        goto end;
    }

    status = sixel_decoder_decode(decoder);
    sixel_encoder_log_stage(encoder,
                            NULL,
                            "main",
                            "encoder",
                            SIXEL_FAILED(status)
                                ? "png_decode_failed"
                                : "png_decode_done",
                            "status=%d",
                            status);

end:
    sixel_decoder_unref(decoder);

    return status;
}


/* load source data from specified file and encode it to SIXEL format
 * output to encoder->outfd */
SIXELAPI SIXELSTATUS
sixel_encoder_encode(
    sixel_encoder_t *encoder,   /* encoder object */
    char const      *filename)  /* input filename */
{
    SIXELSTATUS status = SIXEL_FALSE;
    SIXELSTATUS palette_status = SIXEL_OK;
    int fuse_palette = 1;
    sixel_loader_t *loader = NULL;
    sixel_allocator_t *encode_allocator = NULL;
    char const *png_final_path = NULL;
    char *png_temp_path = NULL;
    size_t png_temp_capacity = 0u;
#if !SIXEL_ENCODER_USE_MKSTEMP_PNG_STAGING
    char *png_tmpnam_result = NULL;
    int png_open_flags = 0;
#endif
#if defined(O_EXCL) && !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    int png_retry_flags = 0;
#endif
#if !SIXEL_ENCODER_USE_MKSTEMP_PNG_STAGING
    int png_open_attempt;
#endif
#if !SIXEL_ENCODER_USE_MKSTEMP_PNG_STAGING || defined(O_BINARY)
    int png_open_errno;
#endif
    sixel_clipboard_spec_t clipboard_spec;
    char clipboard_input_format[32];
    char *clipboard_input_path;
    unsigned char *clipboard_blob;
    size_t clipboard_blob_size;
    SIXELSTATUS clipboard_status;
    char const *effective_filename;
    unsigned int path_flags;
    int path_check;
    sixel_timeline_logger_t *logger;
    int logger_prepared;
    sixel_encoder_load_context_t load_context;
    SIXELSTATUS pipeline_wait_status;
    int saved_errno;
    int saved_lut_policy;
    int force_auto_lut_for_psd;
    int psd_trace_only;

    clipboard_input_format[0] = '\0';
    clipboard_input_path = NULL;
    clipboard_blob = NULL;
    clipboard_blob_size = 0u;
    clipboard_status = SIXEL_OK;
    effective_filename = filename;
    path_flags = SIXEL_OPTION_PATH_ALLOW_STDIN |
        SIXEL_OPTION_PATH_ALLOW_CLIPBOARD |
        SIXEL_OPTION_PATH_ALLOW_REMOTE;
    path_check = 0;
    logger_prepared = 0;
    pipeline_wait_status = SIXEL_OK;
#if !SIXEL_ENCODER_USE_MKSTEMP_PNG_STAGING
    png_open_attempt = 0;
#endif
#if !SIXEL_ENCODER_USE_MKSTEMP_PNG_STAGING || defined(O_BINARY)
    png_open_errno = 0;
#endif
    saved_errno = 0;
    saved_lut_policy = SIXEL_LUT_POLICY_AUTO;
    force_auto_lut_for_psd = 0;
    psd_trace_only = 0;
    memset(&load_context, 0, sizeof(load_context));
    logger = NULL;
    if (encoder == NULL) {
        status = sixel_encoder_new(&encoder, NULL);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: sixel_encoder_new() failed.");
            goto end;
        }
    }
    if (encoder == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    sixel_encoder_ref(encoder);
    sixel_encoder_clear_dither_cache(encoder);
    (void)sixel_timeline_logger_prepare_env(encoder->allocator, &logger);
    logger_prepared = logger != NULL;

    if (encoder != NULL) {
        encoder->logger = logger;
        encoder->parallel_job_id = -1;
        sixel_encoder_update_diagnostic_dither(encoder, NULL);
        load_context.encoder = encoder;
        load_context.output = NULL;
        load_context.planner = &encoder->planner;
        status = sixel_encoder_frame_pipeline_init(&load_context.frame_pipeline,
                                                   encoder,
                                                   NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_encoder_log_stage(encoder,
                                NULL,
                                "main",
                                "encoder",
                                "encode_begin",
                                "input=%s",
                                filename != NULL ? filename : "(stdin)");
    }

    if (filename != NULL) {
        path_check = sixel_option_validate_filesystem_path(
            filename,
            filename,
            path_flags);
        if (path_check != 0) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
    }

    if (encoder != NULL) {
        encode_allocator = encoder->allocator;
        if (encode_allocator != NULL) {
            /*
             * Hold a reference until cleanup so worker side-effects or loader
             * destruction cannot release the allocator before sequential
             * teardown finishes using it.
             */
            sixel_allocator_ref(encode_allocator);
        }
    }

    clipboard_spec.is_clipboard = 0;
    clipboard_spec.format[0] = '\0';
    if (effective_filename != NULL
            && sixel_clipboard_parse_spec(effective_filename,
                                          &clipboard_spec)
            && clipboard_spec.is_clipboard) {
        clipboard_select_format(clipboard_input_format,
                                sizeof(clipboard_input_format),
                                clipboard_spec.format,
                                "sixel");
        clipboard_status = sixel_clipboard_read(
            clipboard_input_format,
            &clipboard_blob,
            &clipboard_blob_size,
            encoder->allocator);
        if (SIXEL_FAILED(clipboard_status)) {
            status = clipboard_status;
            goto end;
        }
        clipboard_status = clipboard_create_spool(
            encoder->allocator,
            "clipboard-in",
            &clipboard_input_path,
            NULL);
        if (SIXEL_FAILED(clipboard_status)) {
            status = clipboard_status;
            goto end;
        }
        clipboard_status = clipboard_write_file(
            clipboard_input_path,
            clipboard_blob,
            clipboard_blob_size);
        if (SIXEL_FAILED(clipboard_status)) {
            status = clipboard_status;
            goto end;
        }
        if (clipboard_blob != NULL) {
            free(clipboard_blob);
            clipboard_blob = NULL;
        }
        effective_filename = clipboard_input_path;
    }

    if (sixel_encoder_should_force_auto_lut_for_psd(encoder,
                                                    effective_filename)) {
        saved_lut_policy = encoder->lut_policy;
        encoder->lut_policy = SIXEL_LUT_POLICY_AUTO;
        force_auto_lut_for_psd = 1;
        sixel_encoder_log_stage(encoder,
                                NULL,
                                "main",
                                "encoder",
                                "lut_policy_override",
                                "source=%s policy=auto reason=psd-default",
                                effective_filename != NULL
                                    ? effective_filename
                                    : "(null)");
    }
    psd_trace_only = sixel_encoder_should_use_psd_trace_only(
        effective_filename);
    load_context.psd_trace_only = psd_trace_only;

    if (encoder->output_is_png) {
#if !SIXEL_ENCODER_USE_MKSTEMP_PNG_STAGING
        png_tmpnam_result = NULL;
#endif
        sixel_encoder_log_stage(encoder,
                                NULL,
                                "main",
                                "encoder",
                                "png_temp_prepare_begin",
                                "target=%s",
                                encoder->png_output_path != NULL
                                    ? encoder->png_output_path
                                    : "(stdout)");
        png_temp_capacity = 0u;
        png_temp_path = create_temp_template(encoder->allocator,
                                             &png_temp_capacity);
        if (png_temp_path == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: malloc() failed for PNG staging path.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        temp_debug_log("png_template_allocated", png_temp_path, 0);
#if SIXEL_ENCODER_USE_MKSTEMP_PNG_STAGING
        if (encoder->outfd >= 0 && encoder->outfd != STDOUT_FILENO) {
            (void)sixel_compat_close(encoder->outfd);
        }
        /*
         * NODERAWFS can race when we reserve a name with mktemp() and reopen
         * it later. Keep the mkstemp() descriptor for PNG staging so the
         * temp path remains owned by this process until write_png_from_sixel()
         * finishes decoding it back out to the final PNG target.
         */
        encoder->outfd = mkstemp(png_temp_path);
        if (encoder->outfd < 0) {
            temp_debug_log("png_mkstemp_open_failed", png_temp_path, errno);
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: failed to create the PNG target file.");
            status = SIXEL_LIBC_ERROR;
            goto end;
        }
# if defined(O_BINARY)
        if (setmode(encoder->outfd, O_BINARY) < 0) {
            png_open_errno = errno;
            temp_debug_log("png_mkstemp_setmode_failed",
                           png_temp_path,
                           png_open_errno);
            (void)sixel_compat_close(encoder->outfd);
            encoder->outfd = (-1);
            errno = png_open_errno;
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: failed to create the PNG target file.");
            status = SIXEL_LIBC_ERROR;
            goto end;
        }
# endif
#else
        if (sixel_compat_mktemp(png_temp_path, png_temp_capacity) != 0) {
            temp_debug_log("png_mktemp_failed", png_temp_path, errno);
            png_tmpnam_result = sixel_compat_tmpnam(png_temp_path,
                                                   png_temp_capacity);
            if (png_tmpnam_result == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_encode: mktemp() failed for PNG staging file.");
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            png_temp_capacity = strlen(png_temp_path) + 1u;
            temp_debug_log("png_tmpnam_fallback", png_temp_path, 0);
        }
        if (encoder->outfd >= 0 && encoder->outfd != STDOUT_FILENO) {
            (void)sixel_compat_close(encoder->outfd);
        }
        png_open_flags = O_RDWR | O_CREAT | O_TRUNC;
#if defined(O_BINARY)
        png_open_flags |= O_BINARY;
#endif
        /*
         * Keep the staging open sequence aligned with clipboard spool
         * creation and avoid Windows O_EXCL false negatives on generated temp
         * paths.
         */
        /*
         * Emscripten + NODERAWFS can report false EEXIST for O_EXCL on
         * freshly generated temp names. Keep O_EXCL on native runtimes
         * and use O_CREAT|O_TRUNC for Emscripten staging files.
         */
#if defined(O_EXCL) && !defined(_WIN32) && !defined(__EMSCRIPTEN__)
        png_open_flags |= O_EXCL;
        png_retry_flags = png_open_flags;
        png_retry_flags &= ~O_EXCL;
#endif
        png_open_errno = 0;
        for (png_open_attempt = 0; png_open_attempt < 4; ++png_open_attempt) {
            encoder->outfd = sixel_compat_open(png_temp_path,
                                               png_open_flags,
                                               S_IRUSR | S_IWUSR);
#if defined(O_EXCL) && !defined(_WIN32) && !defined(__EMSCRIPTEN__)
            if (encoder->outfd < 0
                    && (errno == EBADF
                        || errno == EINVAL
                        || errno == ENOENT
                        || errno == EACCES
                        || errno == EPERM)) {
                /*
                 * Some runtimes can reject O_EXCL with transient errno values
                 * even when the generated path is valid. Retry without O_EXCL
                 * before treating the open as fatal.
                 */
                encoder->outfd = sixel_compat_open(png_temp_path,
                                                   png_retry_flags,
                                                   S_IRUSR | S_IWUSR);
            }
#endif
            if (encoder->outfd >= 0) {
                break;
            }
            png_open_errno = errno;
            temp_debug_log("png_open_failed", png_temp_path, png_open_errno);
            if (png_open_errno != EEXIST) {
                break;
            }

            /*
             * mktemp() reserves a path, but another worker can still claim
             * the same name before open(). Regenerate and retry on EEXIST.
             */
            if (sixel_compat_mktemp(png_temp_path, png_temp_capacity) != 0) {
                temp_debug_log("png_mktemp_retry_failed",
                               png_temp_path,
                               errno);
                png_tmpnam_result = sixel_compat_tmpnam(png_temp_path,
                                                        png_temp_capacity);
                if (png_tmpnam_result == NULL) {
                    break;
                }
                png_temp_capacity = strlen(png_temp_path) + 1u;
                temp_debug_log("png_tmpnam_retry", png_temp_path, 0);
            }
        }
        if (encoder->outfd < 0) {
            if (png_open_errno != 0) {
                errno = png_open_errno;
            }
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: failed to create the PNG target file.");
            status = SIXEL_LIBC_ERROR;
            goto end;
        }
#endif
        sixel_encoder_log_stage(encoder,
                                NULL,
                                "main",
                                "encoder",
                                "png_temp_prepare_done",
                                "temp=%s",
                                png_temp_path != NULL
                                    ? png_temp_path
                                    : "(null)");
    }

    if (encode_allocator == NULL && encoder != NULL) {
        encode_allocator = encoder->allocator;
        if (encode_allocator != NULL) {
            /* Ensure the allocator stays valid after lazy encoder creation. */
            sixel_allocator_ref(encode_allocator);
        }
    }

    /* if required color is not set, set the max value */
    if (encoder->reqcolors == (-1)) {
        encoder->reqcolors = SIXEL_PALETTE_MAX;
    }

    /* if required color is less then 2, set the min value */
    if (encoder->reqcolors < 2) {
        encoder->reqcolors = SIXEL_PALETTE_MIN;
    }

    /* if color space option is not set, choose RGB color space */
    if (encoder->palette_type == SIXEL_PALETTETYPE_AUTO) {
        encoder->palette_type = SIXEL_PALETTETYPE_RGB;
    }

    /* if color option is not default value, prohibit to read
       the file as a paletted image */
    if (encoder->color_option != SIXEL_COLOR_OPTION_DEFAULT) {
        fuse_palette = 0;
    }

    /* if scaling options are set, prohibit to read the file as
       a paletted image */
    if (encoder->percentwidth > 0 ||
        encoder->percentheight > 0 ||
        encoder->pixelwidth > 0 ||
        encoder->pixelheight > 0) {
        fuse_palette = 0;
    }

reload:

    sixel_helper_set_thumbnail_size_hint(
        sixel_encoder_thumbnail_hint(encoder));

    status = sixel_loader_new(&loader, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                 &encoder->fstatic);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_USE_PALETTE,
                                 &fuse_palette);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQCOLORS,
                                 &encoder->reqcolors);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_BGCOLOR,
                                 encoder->bgcolor);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }
    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_BGCOLOR_SOURCE,
                                 &encoder->bgcolor_source);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                 &encoder->loop_mode);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_INSECURE,
                                 &encoder->finsecure);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CANCEL_FLAG,
                                 encoder->cancel_flag);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }
    status = sixel_loader_set_cancel_callback(loader,
                                              encoder->cancel_function,
                                              encoder->cancel_context);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 encoder->loader_order);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_START_FRAME_NO,
                                 encoder->loader_start_frame_no_set
                                     ? &encoder->loader_start_frame_no
                                     : NULL);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 &load_context);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_load_file(loader,
                                    effective_filename,
                                    load_image_callback);
    sixel_encoder_handoff_trace_emit(
        &load_context.frame_pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENCODE_LOADER_RESULT,
        -1,
        -1,
        status,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
    if (status != SIXEL_OK) {
        sixel_encoder_log_stage(encoder,
                                NULL,
                                "main",
                                "encoder",
                                "loader_failed",
                                "status=%d source=%s",
                                status,
                                effective_filename != NULL
                                    ? effective_filename
                                    : "(null)");
        goto load_end;
    }
load_end:
    sixel_loader_unref(loader);
    loader = NULL;

    sixel_encoder_handoff_trace_emit(
        &load_context.frame_pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENCODE_PIPELINE_FINISH_BEGIN,
        -1,
        -1,
        status,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
    pipeline_wait_status = sixel_encoder_frame_pipeline_finish(
        &load_context.frame_pipeline);
    sixel_encoder_handoff_trace_emit(
        &load_context.frame_pipeline,
        SIXEL_ENCODER_HANDOFF_TRACE_EVENT_ENCODE_PIPELINE_FINISH_END,
        -1,
        -1,
        pipeline_wait_status,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_NONE);
    if (status == SIXEL_OK && pipeline_wait_status != SIXEL_OK) {
        status = pipeline_wait_status;
    }

    if (status != SIXEL_OK) {
        goto end;
    }

    if (psd_trace_only != 0 &&
        encoder->output_is_png == 0 &&
        encoder->clipboard_output_active == 0 &&
        encoder->pipe_mode == 0) {
        sixel_encoder_log_stage(
            encoder,
            NULL,
            "main",
            "encoder",
            "psd_trace_only_done",
            "source=%s",
            effective_filename != NULL ? effective_filename : "(null)");
        goto end;
    }

    palette_status = sixel_encoder_emit_palette_output(encoder);
    if (SIXEL_FAILED(palette_status)) {
        sixel_encoder_log_stage(encoder,
                                NULL,
                                "main",
                                "encoder",
                                "palette_emit_failed",
                                "status=%d",
                                palette_status);
        status = palette_status;
        goto end;
    }

    if (encoder->pipe_mode) {
#if HAVE_CLEARERR
        clearerr(stdin);
#endif  /* HAVE_FSEEK */
        while (sixel_encoder_is_cancel_requested(encoder) == 0) {
            status = sixel_tty_wait_stdin(1000000);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            if (status != SIXEL_OK) {
                break;
            }
        }
        if (sixel_encoder_is_cancel_requested(encoder) == 0) {
            goto reload;
        }
    }

    if (encoder->output_is_png) {
        if (encoder->outfd >= 0
                && encoder->outfd != STDOUT_FILENO
                && encoder->outfd != STDERR_FILENO) {
            /*
             * Close the staging descriptor before decoding so Windows CRT
             * share-mode semantics cannot block readback of the temporary
             * SIXEL payload.
             */
            (void)sixel_compat_close(encoder->outfd);
            encoder->outfd = STDOUT_FILENO;
        }
        png_final_path = encoder->output_png_to_stdout
            ? "-"
            : encoder->png_output_path;
        if (! encoder->output_png_to_stdout && png_final_path == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: missing PNG output path.");
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }
        status = write_png_from_sixel(png_temp_path, png_final_path, encoder);
        sixel_encoder_log_stage(encoder,
                                NULL,
                                "main",
                                "encoder",
                                SIXEL_FAILED(status)
                                    ? "png_emit_failed"
                                    : "png_emit_done",
                                "status=%d",
                                status);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (encoder->clipboard_output_active
            && encoder->clipboard_output_path != NULL) {
        unsigned char *clipboard_output_data;
        size_t clipboard_output_size;

        clipboard_output_data = NULL;
        clipboard_output_size = 0u;

        if (encoder->outfd
                && encoder->outfd != STDOUT_FILENO
                && encoder->outfd != STDERR_FILENO) {
            (void)sixel_compat_close(encoder->outfd);
            encoder->outfd = STDOUT_FILENO;
        }

        clipboard_status = clipboard_read_file(
            encoder->clipboard_output_path,
            &clipboard_output_data,
            &clipboard_output_size);
        if (SIXEL_SUCCEEDED(clipboard_status)) {
            clipboard_status = sixel_clipboard_write(
                encoder->clipboard_output_format,
                clipboard_output_data,
                clipboard_output_size);
        }
        if (clipboard_output_data != NULL) {
            free(clipboard_output_data);
        }
        if (SIXEL_FAILED(clipboard_status)) {
            status = clipboard_status;
            goto end;
        }
        (void)sixel_compat_unlink(encoder->clipboard_output_path);
        sixel_allocator_free(encoder->allocator,
                             encoder->clipboard_output_path);
        encoder->clipboard_output_path = NULL;
        encoder->sixel_output_path = NULL;
        encoder->clipboard_output_active = 0;
        encoder->clipboard_output_format[0] = '\0';
    }

    /* the status may not be SIXEL_OK */

end:
    saved_errno = errno;
    if (encoder != NULL && force_auto_lut_for_psd != 0) {
        encoder->lut_policy = saved_lut_policy;
    }
    (void)sixel_encoder_frame_pipeline_finish(&load_context.frame_pipeline);
    sixel_encoder_frame_pipeline_dispose(&load_context.frame_pipeline);
    (void)sixel_tty_end_animation_input_guard();
    if (encoder != NULL) {
        (void)sixel_tty_restore_cursor(encoder->outfd);
        sixel_encoder_emit_dither_contract(encoder, status);
        sixel_encoder_emit_palette_contract(encoder, status);
    }
    if (encoder != NULL) {
        sixel_encoder_log_stage(encoder,
                                NULL,
                                "main",
                                "encoder",
                                SIXEL_FAILED(status)
                                    ? "encode_failed"
                                    : "encode_done",
                                "status=%d",
                                status);
    }
    if (png_temp_path != NULL) {
        (void)sixel_compat_unlink(png_temp_path);
    }
    if (encoder != NULL) {
        sixel_allocator_free(encoder->allocator, png_temp_path);
    }
    if (clipboard_input_path != NULL) {
        (void)sixel_compat_unlink(clipboard_input_path);
        if (encoder != NULL) {
            sixel_allocator_free(encoder->allocator, clipboard_input_path);
        }
    }
    if (clipboard_blob != NULL) {
        free(clipboard_blob);
    }
    if (encoder != NULL && encoder->clipboard_output_path != NULL) {
        (void)sixel_compat_unlink(encoder->clipboard_output_path);
        sixel_allocator_free(encoder->allocator,
                             encoder->clipboard_output_path);
        encoder->clipboard_output_path = NULL;
        encoder->sixel_output_path = NULL;
        encoder->clipboard_output_active = 0;
        encoder->clipboard_output_format[0] = '\0';
    }
    if (encoder != NULL) {
        sixel_allocator_free(encoder->allocator, encoder->png_output_path);
        encoder->png_output_path = NULL;
    }
    if (encoder != NULL) {
        encoder->logger = NULL;
        encoder->parallel_job_id = -1;
        sixel_encoder_clear_dither_cache(encoder);
    }
    if (logger_prepared) {
        sixel_timeline_logger_unref(logger);
    }

    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }

    if (encode_allocator != NULL) {
        /*
         * Release the retained allocator reference *after* dropping the
         * encoder reference so that a lazily created encoder can run its
         * destructor while the allocator is still alive.  This ensures that
         * cleanup routines never dereference a freed allocator instance.
         */
        sixel_allocator_unref(encode_allocator);
        encode_allocator = NULL;
    }
    errno = saved_errno;

    return status;
}


/* encode specified pixel data to SIXEL format
 * output to encoder->outfd */
SIXELAPI SIXELSTATUS
sixel_encoder_encode_bytes(
    sixel_encoder_t     /* in */    *encoder,
    unsigned char       /* in */    *bytes,
    int                 /* in */    width,
    int                 /* in */    height,
    int                 /* in */    pixelformat,
    unsigned char       /* in */    *palette,
    int                 /* in */    ncolors)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;
    unsigned char *owned_pixels = NULL;
    unsigned char *owned_palette = NULL;
    unsigned char *normalized_pixels = NULL;
    size_t pixel_bytes;
    size_t pixel_total;
    size_t palette_bytes;
    size_t normalized_bytes;
    int depth;
    int normalized_pixelformat;
    int normalize_packed_pixels;

    normalized_pixels = NULL;
    if (encoder == NULL || bytes == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    depth = sixel_helper_compute_depth(pixelformat);
    if (depth <= 0) {
        sixel_helper_set_additional_message(
            "sixel_encoder_encode_bytes: invalid pixelformat depth.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    pixel_total = (size_t)width * (size_t)height;
    if (width <= 0 || height <= 0 ||
            pixel_total / (size_t)width != (size_t)height) {
        sixel_helper_set_additional_message(
            "sixel_encoder_encode_bytes: invalid frame dimensions.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (width > SIXEL_WIDTH_LIMIT || height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_encoder_encode_bytes: frame dimensions exceed limits.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    status = sixel_encoder_compute_frame_buffer_bytes(width,
                                                      height,
                                                      pixelformat,
                                                      &pixel_bytes);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_encoder_encode_bytes: buffer size overflow.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    owned_pixels = (unsigned char *)sixel_allocator_malloc(
        encoder->allocator, pixel_bytes);
    if (owned_pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_encode_bytes: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(owned_pixels, bytes, pixel_bytes);

    normalize_packed_pixels = 0;
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_G1:
    case SIXEL_PIXELFORMAT_G2:
    case SIXEL_PIXELFORMAT_G4:
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
        normalize_packed_pixels = 1;
        break;
    default:
        break;
    }

    if (normalize_packed_pixels != 0) {
        normalized_bytes = pixel_total;
        normalized_pixels = (unsigned char *)sixel_allocator_malloc(
            encoder->allocator, normalized_bytes);
        if (normalized_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_bytes: sixel_allocator_malloc() "
                "failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        normalized_pixelformat = pixelformat;
        status = sixel_helper_normalize_pixelformat(normalized_pixels,
                                                    &normalized_pixelformat,
                                                    owned_pixels,
                                                    pixelformat,
                                                    width,
                                                    height);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_bytes: "
                "sixel_helper_normalize_pixelformat() failed.");
            goto end;
        }

        /*
         * Downstream sampling/palette filters index source rows as
         * one-byte-per-pixel buffers. Promote packed inputs here so all
         * encoder paths observe consistent frame storage semantics.
         */
        sixel_allocator_free(encoder->allocator, owned_pixels);
        owned_pixels = normalized_pixels;
        normalized_pixels = NULL;
        pixelformat = normalized_pixelformat;
    }

    palette_bytes = 0u;
    if (pixelformat & SIXEL_FORMATTYPE_PALETTE) {
        if (palette == NULL || ncolors <= 0) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_bytes: missing palette data.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        palette_bytes = (size_t)ncolors * 3u;
        if (palette_bytes / 3u != (size_t)ncolors) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_bytes: palette size overflow.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        owned_palette = (unsigned char *)sixel_allocator_malloc(
            encoder->allocator, palette_bytes);
        if (owned_palette == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_bytes: "
                "sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memcpy(owned_palette, palette, palette_bytes);
    }

    status = sixel_frame_create_from_factory(&frame, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_init(frame, owned_pixels, width, height,
                              pixelformat, owned_palette, ncolors);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    owned_pixels = NULL;
    owned_palette = NULL;

    status = sixel_encoder_encode_frame(encoder, frame, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    (void)sixel_tty_end_animation_input_guard();
    if (encoder != NULL) {
        (void)sixel_tty_restore_cursor(encoder->outfd);
        sixel_allocator_free(encoder->allocator, normalized_pixels);
        sixel_allocator_free(encoder->allocator, owned_pixels);
        sixel_allocator_free(encoder->allocator, owned_palette);
    }
    if (frame != NULL) {
        /*
         * The encoder owns the buffers allocated above, so a single unref
         * must release the frame and its heap allocations exactly once.
         */
        sixel_frame_unref(frame);
    }
    return status;
}


/*
 * Enable or disable the quantized-frame capture facility.
 *
 *     capture on --> encoder keeps the latest palette-quantized frame.
 *     capture off --> encoder forgets previously stored frames.
 */
static SIXELSTATUS
sixel_encoder_enable_quantized_capture_internal(
    sixel_encoder_t *encoder,
    int enable)
{
    if (encoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_enable_quantized_capture: encoder is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    encoder->capture_quantized = enable ? 1 : 0;
    if (!encoder->capture_quantized) {
        encoder->capture_valid = 0;
    }

    return SIXEL_OK;
}


/*
 * Materialize the captured quantized frame as a heap-allocated
 * sixel_frame_t instance.
 */
static SIXELSTATUS
sixel_encoder_copy_quantized_frame_internal(
    sixel_encoder_t   *encoder,
    sixel_allocator_t *allocator,
    sixel_frame_t     **ppframe)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame;
    unsigned char *pixels;
    unsigned char *palette;
    size_t palette_bytes;

    if (encoder == NULL || allocator == NULL || ppframe == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_copy_quantized_frame: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (!encoder->capture_quantized || !encoder->capture_valid) {
        sixel_helper_set_additional_message(
            "sixel_encoder_copy_quantized_frame: no frame captured.");
        return SIXEL_RUNTIME_ERROR;
    }

    *ppframe = NULL;
    frame = NULL;
    pixels = NULL;
    palette = NULL;

    status = sixel_frame_create_from_factory(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (encoder->capture_pixel_bytes > 0) {
        pixels = (unsigned char *)sixel_allocator_malloc(
            allocator, encoder->capture_pixel_bytes);
        if (pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_copy_quantized_frame: "
                "sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        memcpy(pixels,
               encoder->capture_pixels,
               encoder->capture_pixel_bytes);
    }

    palette_bytes = encoder->capture_palette_size;
    if (palette_bytes > 0) {
        palette = (unsigned char *)sixel_allocator_malloc(allocator,
                                                          palette_bytes);
        if (palette == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_copy_quantized_frame: "
                "sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        memcpy(palette,
               encoder->capture_palette,
               palette_bytes);
    }

    status = sixel_frame_init(frame,
                              pixels,
                              encoder->capture_width,
                              encoder->capture_height,
                              encoder->capture_pixelformat,
                              palette,
                              encoder->capture_ncolors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    pixels = NULL;
    palette = NULL;
    /*
     * Preserve the captured colorspace via the public setter to avoid
     * depending on frame internals.
     */
    sixel_frame_set_colorspace(frame, encoder->capture_colorspace);
    *ppframe = frame;
    return SIXEL_OK;

cleanup:
    if (palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (pixels != NULL) {
        sixel_allocator_free(allocator, pixels);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    return status;
}


/*
 * Emit the captured palette in the requested format.
 *
 *   palette_output == NULL  -> skip
 *   palette_output != NULL  -> materialize captured palette
 */
static SIXELSTATUS
sixel_encoder_emit_palette_output(sixel_encoder_t *encoder)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char const *palette;
    size_t palette_bytes;
    int exported_colors;
    FILE *stream;
    int close_stream;
    char const *path;
    sixel_palette_format_t format_hint;
    sixel_palette_format_t format_ext;
    sixel_palette_format_t format_final;
    char const *mode;
    char *libc_buffer;
    size_t libc_buffer_size;
    char const *libc_path;

    status = SIXEL_OK;
    frame = NULL;
    palette = NULL;
    palette_bytes = 0u;
    exported_colors = 0;
    stream = NULL;
    close_stream = 0;
    path = NULL;
    format_hint = SIXEL_PALETTE_FORMAT_NONE;
    format_ext = SIXEL_PALETTE_FORMAT_NONE;
    format_final = SIXEL_PALETTE_FORMAT_NONE;
    mode = "wb";
    libc_buffer = NULL;
    libc_buffer_size = 0u;
    libc_path = NULL;

    if (encoder == NULL || encoder->palette_output == NULL) {
        return SIXEL_OK;
    }

    status = sixel_encoder_copy_quantized_frame_internal(encoder,
                                                         encoder->allocator,
                                                         &frame);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    palette = (unsigned char const *)sixel_frame_get_palette(frame);
    exported_colors = sixel_frame_get_ncolors(frame);
    if (exported_colors > 0) {
        palette_bytes = (size_t)exported_colors * 3u;
    }
    if (palette == NULL || exported_colors <= 0) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_palette_output: palette unavailable.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    if (exported_colors > 256) {
        exported_colors = 256;
    }

    path = sixel_palette_strip_prefix(encoder->palette_output, &format_hint);
    if (path == NULL || *path == '\0') {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_palette_output: invalid path.");
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    format_ext = sixel_palette_format_from_extension(path);
    format_final = format_hint;
    if (format_final == SIXEL_PALETTE_FORMAT_NONE) {
        if (format_ext == SIXEL_PALETTE_FORMAT_NONE) {
            if (strcmp(path, "-") == 0) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_emit_palette_output: "
                    "format required for '-'.");
                status = SIXEL_BAD_ARGUMENT;
                goto cleanup;
            }
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: "
                "unknown palette file extension.");
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }
        format_final = format_ext;
    }
    if (format_final == SIXEL_PALETTE_FORMAT_PAL_AUTO) {
        format_final = SIXEL_PALETTE_FORMAT_PAL_JASC;
    }

    libc_buffer_size = sixel_path_to_libc_buffer_size(path);
    if (libc_buffer_size > 0u) {
        libc_buffer = (char *)sixel_allocator_malloc(encoder->allocator,
                                                     libc_buffer_size);
        if (libc_buffer == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: "
                "sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        libc_path = sixel_path_to_libc(path, libc_buffer, libc_buffer_size);
        if (libc_path == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: invalid path.");
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }
        if (libc_path == libc_buffer) {
            path = libc_buffer;
            libc_buffer = NULL;
        }
    }

    if (strcmp(path, "-") == 0) {
        stream = stdout;
    } else {
        if (format_final == SIXEL_PALETTE_FORMAT_PAL_JASC ||
                format_final == SIXEL_PALETTE_FORMAT_GPL) {
            mode = "w";
        } else {
            mode = "wb";
        }
        stream = sixel_compat_fopen(path, mode);
        if (stream == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: failed to open file.");
            status = SIXEL_LIBC_ERROR;
            goto cleanup;
        }
        close_stream = 1;
    }

    switch (format_final) {
    case SIXEL_PALETTE_FORMAT_ACT:
        status = sixel_palette_write_act(stream,
                                         palette,
                                         palette_bytes,
                                         exported_colors);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: failed to write ACT.");
        }
        break;
    case SIXEL_PALETTE_FORMAT_PAL_JASC:
        status = sixel_palette_write_pal_jasc(stream,
                                              palette,
                                              exported_colors);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: failed to write JASC.");
        }
        break;
    case SIXEL_PALETTE_FORMAT_PAL_RIFF:
        status = sixel_palette_write_pal_riff(stream,
                                              palette,
                                              exported_colors);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: failed to write RIFF.");
        }
        break;
    case SIXEL_PALETTE_FORMAT_GPL:
        status = sixel_palette_write_gpl(stream,
                                         palette,
                                         exported_colors);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: failed to write GPL.");
        }
        break;
    default:
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_palette_output: unsupported format.");
        status = SIXEL_BAD_ARGUMENT;
        break;
    }
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (close_stream) {
        if (fclose(stream) != 0) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: fclose() failed.");
            status = SIXEL_LIBC_ERROR;
            stream = NULL;
            goto cleanup;
        }
        stream = NULL;
    } else {
        sixel_trace_topic_message("lifecycle",
            "palette output flush begin: stream=%p",
            (void *)stream);
        if (fflush(stream) != 0) {
            sixel_trace_topic_message("lifecycle",
                "palette output flush failed: errno=%d",
                errno);
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: fflush() failed.");
            status = SIXEL_LIBC_ERROR;
            goto cleanup;
        }
        sixel_trace_topic_message("lifecycle",
            "palette output flush end: success");
    }

cleanup:
    if (libc_buffer != NULL) {
        sixel_allocator_free(encoder->allocator, libc_buffer);
        libc_buffer = NULL;
    }
    if (close_stream && stream != NULL) {
        (void) fclose(stream);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }

    return status;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
