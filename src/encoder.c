/*
 * SPDX-License-Identifier: MIT AND BSD-3-Clause
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
 *
 * -------------------------------------------------------------------------------
 * Portions of this file(sixel_encoder_emit_drcsmmv2_chars) are derived from
 * mlterm's drcssixel.c.
 *
 * Copyright (c) Araki Ken(arakiken@users.sourceforge.net)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
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
#include "tty.h"
#include "encoder.h"
#include "frame.h"
#include "output.h"
#include "logger.h"
#include "options.h"
#include "dither.h"
#include "palette-kmeans.h"
#include "palette-common-merge.h"
#include "pixelformat.h"
#include "rgblookup.h"
#include "clipboard.h"
#include "compat_stub.h"
#include "path.h"
#include "filter.h"
#include "filter-clip.h"
#include "filter-colors.h"
#include "filter-dither.h"
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

#define SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY 4
#define SIXEL_ENCODER_HANDOFF_UNDECIDED 0
#define SIXEL_ENCODER_HANDOFF_SERIAL 1
#define SIXEL_ENCODER_HANDOFF_PIPELINE 2

typedef struct sixel_palette_async_job {
    sixel_thread_t thread;
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    sixel_encoder_t *encoder;
    sixel_logger_t *logger;
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
    int started;
    int finished;
} sixel_palette_async_job_t;

typedef struct sixel_palette_builder_context {
    sixel_encoder_t *encoder;
    int allow_cache;
} sixel_palette_builder_context_t;

typedef struct sixel_encoder_frame_pipeline {
    sixel_thread_t thread;
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    sixel_frame_t *queue[SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY];
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    SIXELSTATUS worker_status;
    int queue_head;
    int queue_tail;
    int queue_count;
    int initialized;
    int started;
    int loader_done;
    int handoff_mode;
} sixel_encoder_frame_pipeline_t;

typedef struct sixel_encoder_load_context {
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_encoding_planner_t *planner;
    sixel_encoder_frame_pipeline_t frame_pipeline;
} sixel_encoder_load_context_t;

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
    sixel_logger_t *logger);
static void sixel_debug_print_palette(sixel_dither_t *dither);
static SIXELSTATUS sixel_encoder_output_without_macro(
    sixel_frame_t *frame,
    sixel_dither_t *dither,
    sixel_output_t *output,
    sixel_encoder_t *encoder);
static SIXELSTATUS sixel_encoder_output_with_macro(
    sixel_frame_t *frame,
    sixel_dither_t *dither,
    sixel_output_t *output,
    sixel_encoder_t *encoder);
static SIXELSTATUS sixel_encoder_emit_iso2022_chars(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame);
static SIXELSTATUS sixel_encoder_emit_drcsmmv2_chars(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame);

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

# if defined(CLOCKS_PER_SEC)
#  undef CLOCKS_PER_SEC
# endif
# define CLOCKS_PER_SEC 1000

# if !defined(HAVE_CLOCK)
# define HAVE_CLOCK_WIN 1
static sixel_clock_t
clock_win(void)
{
    FILETIME ct, et, kt, ut;
    ULARGE_INTEGER u, k;

    if (! GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut)) {
        return (sixel_clock_t)(-1);
    }
    u.LowPart = ut.dwLowDateTime; u.HighPart = ut.dwHighDateTime;
    k.LowPart = kt.dwLowDateTime; k.HighPart = kt.dwHighDateTime;
    /* 100ns -> ms */
    return (sixel_clock_t)((u.QuadPart + k.QuadPart) / 10000ULL);
}
# endif  /* HAVE_CLOCK */

#endif /* _WIN32 */


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

static sixel_option_choice_t const g_option_choices_diffusion[] = {
    { "auto", SIXEL_DIFFUSE_AUTO },
    { "none", SIXEL_DIFFUSE_NONE },
    { "fs", SIXEL_DIFFUSE_FS },
    { "atkinson", SIXEL_DIFFUSE_ATKINSON },
    { "jajuni", SIXEL_DIFFUSE_JAJUNI },
    { "stucki", SIXEL_DIFFUSE_STUCKI },
    { "burkes", SIXEL_DIFFUSE_BURKES },
    { "sierra1", SIXEL_DIFFUSE_SIERRA1 },
    { "sierra2", SIXEL_DIFFUSE_SIERRA2 },
    { "sierra3", SIXEL_DIFFUSE_SIERRA3 },
    { "a_dither", SIXEL_DIFFUSE_A_DITHER },
    { "x_dither", SIXEL_DIFFUSE_X_DITHER },
    { "bluenoise", SIXEL_DIFFUSE_BLUENOISE_DITHER },
    { "lso2", SIXEL_DIFFUSE_LSO2 },
};

static sixel_option_choice_t const g_option_choices_diffusion_scan[] = {
    { "auto", SIXEL_SCAN_AUTO },
    { "serpentine", SIXEL_SCAN_SERPENTINE },
    { "raster", SIXEL_SCAN_RASTER }
};

static sixel_option_choice_t const g_option_choices_diffusion_carry[] = {
    { "auto", SIXEL_CARRY_AUTO },
    { "direct", SIXEL_CARRY_DISABLE },
    { "carry", SIXEL_CARRY_ENABLE }
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

static sixel_suboption_key_t const g_subkeys_quantize_model_kmeans[] = {
    {
        "inittype",
        "i",
        "SIXEL_PALETTE_KMEANS_INITTYPE",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_option_choices_kmeans_init_type,
        sizeof(g_option_choices_kmeans_init_type)
        / sizeof(g_option_choices_kmeans_init_type[0])
    },
    {
        "threshold",
        "t",
        "SIXEL_PALETTE_KMEANS_THRESHOLD",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    }
};

static sixel_option_value_schema_t const g_schema_quantize_model_values[] = {
    {
        "auto",
        SIXEL_QUANTIZE_MODEL_AUTO,
        NULL,
        0u
    },
    {
        "heckbert",
        SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        NULL,
        0u
    },
    {
        "kmeans",
        SIXEL_QUANTIZE_MODEL_KMEANS,
        g_subkeys_quantize_model_kmeans,
        sizeof(g_subkeys_quantize_model_kmeans)
        / sizeof(g_subkeys_quantize_model_kmeans[0])
    }
};

static sixel_option_argument_schema_t const g_schema_quantize_model = {
    SIXEL_OPTFLAG_QUANTIZE_MODEL,
    "--quantize-model",
    g_schema_quantize_model_values,
    sizeof(g_schema_quantize_model_values)
    / sizeof(g_schema_quantize_model_values[0])
};

static sixel_option_choice_t const g_option_choices_final_merge[] = {
    { "auto", SIXEL_FINAL_MERGE_AUTO },
    { "none", SIXEL_FINAL_MERGE_NONE },
    { "ward", SIXEL_FINAL_MERGE_WARD }
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

enum {
    SIXEL_LOADER_CHOICE_LIBPNG = 0,
    SIXEL_LOADER_CHOICE_LIBJPEG,
    SIXEL_LOADER_CHOICE_LIBWEBP,
    SIXEL_LOADER_CHOICE_LIBTIFF,
    SIXEL_LOADER_CHOICE_LIBRSVG,
    SIXEL_LOADER_CHOICE_BUILTIN,
    SIXEL_LOADER_CHOICE_WIC,
    SIXEL_LOADER_CHOICE_COREGRAPHICS,
    SIXEL_LOADER_CHOICE_GDK_PIXBUF2,
    SIXEL_LOADER_CHOICE_GD,
    SIXEL_LOADER_CHOICE_QUICKLOOK,
    SIXEL_LOADER_CHOICE_GNOME_THUMBNAILER
};

#if HAVE_WIC
static sixel_suboption_key_t const g_subkeys_loader_wic[] = {
    {
        "ico_minsize",
        NULL,
        "SIXEL_LODER_WIC_ICO_MINSIZE",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    }
};
#endif

static sixel_option_value_schema_t const g_schema_loader_values[] = {
#if HAVE_LIBPNG
    { "libpng", SIXEL_LOADER_CHOICE_LIBPNG, NULL, 0u },
#endif
#if HAVE_JPEG
    { "libjpeg", SIXEL_LOADER_CHOICE_LIBJPEG, NULL, 0u },
#endif
#if HAVE_WEBP
    { "libwebp", SIXEL_LOADER_CHOICE_LIBWEBP, NULL, 0u },
#endif
#if HAVE_LIBTIFF
    { "libtiff", SIXEL_LOADER_CHOICE_LIBTIFF, NULL, 0u },
#endif
#if HAVE_LIBRSVG
    { "librsvg", SIXEL_LOADER_CHOICE_LIBRSVG, NULL, 0u },
#endif
    { "builtin", SIXEL_LOADER_CHOICE_BUILTIN, NULL, 0u },
#if HAVE_WIC
    {
        "wic",
        SIXEL_LOADER_CHOICE_WIC,
        g_subkeys_loader_wic,
        sizeof(g_subkeys_loader_wic) / sizeof(g_subkeys_loader_wic[0])
    },
#endif
#if HAVE_COREGRAPHICS
    { "coregraphics", SIXEL_LOADER_CHOICE_COREGRAPHICS, NULL, 0u },
#endif
#ifdef HAVE_GDK_PIXBUF2
    { "gdk-pixbuf2", SIXEL_LOADER_CHOICE_GDK_PIXBUF2, NULL, 0u },
#endif
#if HAVE_GD
    { "gd", SIXEL_LOADER_CHOICE_GD, NULL, 0u },
#endif
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    { "quicklook", SIXEL_LOADER_CHOICE_QUICKLOOK, NULL, 0u },
#endif
#if HAVE_FREEDESKTOP_THUMBNAILING
    {
        "gnome-thumbnailer",
        SIXEL_LOADER_CHOICE_GNOME_THUMBNAILER,
        NULL,
        0u
    },
#endif
};

static sixel_option_argument_schema_t const g_schema_loaders = {
    SIXEL_OPTFLAG_LOADERS,
    "--loaders",
    g_schema_loader_values,
    sizeof(g_schema_loader_values) / sizeof(g_schema_loader_values[0])
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

/*
 * Parse the loader order list:
 *
 *   input := token[,token...][!]
 *
 * The steps are:
 *  1. Trim trailing whitespace and detect the optional "!" suffix.
 *  2. Match each token against the loader choice list (prefix-friendly).
 *  3. Preserve loader suboptions while canonicalizing loader names.
 *  4. Emit the canonical loader order string for the loader registry.
 */
static int
sixel_loader_parse_positive_int(char const *text,
                                size_t length,
                                int *value_out)
{
    size_t index;
    int value;
    unsigned char digit;

    index = 0u;
    value = 0;

    if (text == NULL || value_out == NULL || length == 0u) {
        return 0;
    }

    while (index < length) {
        digit = (unsigned char)text[index];
        if (digit < (unsigned char)'0' || digit > (unsigned char)'9') {
            return 0;
        }
        if (value > (INT_MAX - 9) / 10) {
            return 0;
        }
        value = value * 10 + (digit - (unsigned char)'0');
        ++index;
    }

    if (value <= 0) {
        return 0;
    }

    *value_out = value;
    return 1;
}

static SIXELSTATUS
sixel_encoder_validate_loader_suboptions(
    sixel_option_argument_resolution_t const *resolution)
{
    size_t index;
    sixel_suboption_assignment_t const *assignment;
    int parsed_value;

    index = 0u;
    assignment = NULL;
    parsed_value = 0;

    if (resolution == NULL || resolution->base_def == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (strcmp(resolution->base_def->name, "wic") != 0 &&
        resolution->assignment_count > 0u) {
        sixel_helper_set_additional_message(
            "specified loader does not support suboptions.");
        return SIXEL_BAD_ARGUMENT;
    }

    while (index < resolution->assignment_count) {
        assignment = resolution->assignments + index;
        if (strcmp(assignment->resolved_key_name, "ico_minsize") == 0) {
            if (!sixel_loader_parse_positive_int(
                    assignment->resolved_value_text,
                    strlen(assignment->resolved_value_text),
                    &parsed_value)) {
                sixel_helper_set_additional_message(
                    "invalid wic suboption; expected :ico_minsize=SIZE.");
                return SIXEL_BAD_ARGUMENT;
            }
        }
        ++index;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_parse_loader_order(
    sixel_allocator_t /* in */ *allocator,
    char const        /* in */ *value,
    char              /* out */ **order_out)
{
    SIXELSTATUS status;
    char const *cursor;
    char const *token_start;
    char const *token_end;
    char const *order_end;
    size_t token_length;
    size_t output_length;
    size_t output_used;
    int fallback_disabled;
    int saw_token;
    char token_buffer[256];
    char match_detail[128];
    sixel_option_argument_resolution_t loader_resolution;
    size_t assignment_index;
    size_t text_length;
    char *output;

    status = SIXEL_OK;
    cursor = NULL;
    token_start = NULL;
    token_end = NULL;
    order_end = NULL;
    token_length = 0u;
    output_length = 0u;
    output_used = 0u;
    fallback_disabled = 0;
    saw_token = 0;
    token_buffer[0] = '\0';
    match_detail[0] = '\0';
    loader_resolution.resolved_base_value = 0;
    loader_resolution.base_def = NULL;
    loader_resolution.assignments = NULL;
    loader_resolution.assignment_count = 0u;
    assignment_index = 0u;
    text_length = 0u;
    output = NULL;

    if (order_out != NULL) {
        *order_out = NULL;
    }

    if (value == NULL || value[0] == '\0') {
        return SIXEL_OK;
    }

    order_end = value + strlen(value);
    while (order_end > value &&
           isspace((unsigned char)order_end[-1])) {
        --order_end;
    }
    if (order_end > value && order_end[-1] == '!') {
        fallback_disabled = 1;
        --order_end;
        while (order_end > value &&
               isspace((unsigned char)order_end[-1])) {
            --order_end;
        }
    }
    if (order_end == value) {
        sixel_helper_set_additional_message(
            "loaders option requires at least one loader name.");
        return SIXEL_BAD_ARGUMENT;
    }

    for (cursor = value; cursor < order_end; ++cursor) {
        if (*cursor == '!') {
            sixel_helper_set_additional_message(
                "loaders option only accepts a trailing '!'.");
            return SIXEL_BAD_ARGUMENT;
        }
    }

    cursor = value;
    token_start = value;
    while (cursor <= order_end) {
        if (cursor == order_end || *cursor == ',') {
            token_end = cursor;
            while (token_start < token_end &&
                   isspace((unsigned char)*token_start)) {
                ++token_start;
            }
            while (token_end > token_start &&
                   isspace((unsigned char)token_end[-1])) {
                --token_end;
            }
            token_length = (size_t)(token_end - token_start);
            if (token_length > 0u) {
                if (token_length >= sizeof(token_buffer)) {
                    sixel_helper_set_additional_message(
                        "loader token is too long.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto cleanup;
                }
                memcpy(token_buffer, token_start, token_length);
                token_buffer[token_length] = '\0';
                status = sixel_option_parse_argument_with_suboptions(
                    token_buffer,
                    &g_schema_loaders,
                    &loader_resolution,
                    match_detail,
                    sizeof(match_detail));
                if (SIXEL_FAILED(status)) {
                    goto cleanup;
                }
                status = sixel_encoder_validate_loader_suboptions(
                    &loader_resolution);
                if (SIXEL_FAILED(status)) {
                    goto cleanup;
                }

                if (saw_token) {
                    output_length += 1u;
                }
                output_length += strlen(loader_resolution.base_def->name);
                assignment_index = 0u;
                while (assignment_index < loader_resolution.assignment_count) {
                    output_length += 2u;
                    output_length += strlen(
                        loader_resolution.assignments
                        [assignment_index].resolved_key_name);
                    output_length += strlen(
                        loader_resolution.assignments
                        [assignment_index].resolved_value_text);
                    ++assignment_index;
                }
                saw_token = 1;
                sixel_option_free_argument_resolution(&loader_resolution);
            }
            token_start = cursor + 1;
        }
        ++cursor;
    }

    if (!saw_token) {
        sixel_helper_set_additional_message(
            "loaders option requires at least one loader name.");
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (fallback_disabled) {
        output_length += 1u;
    }

    output = (char *)sixel_allocator_malloc(allocator, output_length + 1u);
    if (output == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_parse_loader_order: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    cursor = value;
    token_start = value;
    output_used = 0u;
    saw_token = 0;
    while (cursor <= order_end) {
        if (cursor == order_end || *cursor == ',') {
            token_end = cursor;
            while (token_start < token_end &&
                   isspace((unsigned char)*token_start)) {
                ++token_start;
            }
            while (token_end > token_start &&
                   isspace((unsigned char)token_end[-1])) {
                --token_end;
            }
            token_length = (size_t)(token_end - token_start);
            if (token_length > 0u) {
                memcpy(token_buffer, token_start, token_length);
                token_buffer[token_length] = '\0';
                status = sixel_option_parse_argument_with_suboptions(
                    token_buffer,
                    &g_schema_loaders,
                    &loader_resolution,
                    match_detail,
                    sizeof(match_detail));
                if (SIXEL_FAILED(status)) {
                    goto cleanup;
                }
                if (saw_token) {
                    output[output_used] = ',';
                    ++output_used;
                }
                text_length = strlen(loader_resolution.base_def->name);
                memcpy(output + output_used,
                       loader_resolution.base_def->name,
                       text_length);
                output_used += text_length;
                assignment_index = 0u;
                while (assignment_index < loader_resolution.assignment_count) {
                    output[output_used] = ':';
                    ++output_used;
                    text_length = strlen(
                        loader_resolution.assignments
                        [assignment_index].resolved_key_name);
                    memcpy(output + output_used,
                           loader_resolution.assignments
                           [assignment_index].resolved_key_name,
                           text_length);
                    output_used += text_length;
                    output[output_used] = '=';
                    ++output_used;
                    text_length = strlen(
                        loader_resolution.assignments
                        [assignment_index].resolved_value_text);
                    memcpy(output + output_used,
                           loader_resolution.assignments
                           [assignment_index].resolved_value_text,
                           text_length);
                    output_used += text_length;
                    ++assignment_index;
                }
                saw_token = 1;
                sixel_option_free_argument_resolution(&loader_resolution);
            }
            token_start = cursor + 1;
        }
        ++cursor;
    }

    if (fallback_disabled) {
        output[output_used] = '!';
        ++output_used;
    }
    output[output_used] = '\0';

    if (order_out != NULL) {
        *order_out = output;
        output = NULL;
    }

cleanup:
    sixel_option_free_argument_resolution(&loader_resolution);
    if (output != NULL) {
        sixel_allocator_free(allocator, output);
    }

    return status;
}

/*
 * Duplicate frame metadata and pixels so palette construction can shift
 * colorspaces without mutating the live frame used for encoding.
 */
static SIXELSTATUS
sixel_encoder_clone_frame(sixel_frame_t *frame,
                          sixel_allocator_t *allocator,
                          sixel_frame_t **frame_out)
{
    SIXELSTATUS status;
    sixel_frame_t *clone;
    unsigned char *pixels;
    unsigned char *palette;
    int palette_bytes;
    int depth_result;
    size_t depth;
    size_t pixel_total;
    size_t pixel_bytes;

    status = SIXEL_BAD_ARGUMENT;
    clone = NULL;
    pixels = NULL;
    palette = NULL;
    palette_bytes = 0;
    depth_result = 0;
    depth = 0U;
    pixel_total = 0U;
    pixel_bytes = 0U;

    if (frame == NULL || frame_out == NULL) {
        return status;
    }

    status = sixel_frame_new(&clone, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    clone->width = frame->width;
    clone->height = frame->height;
    clone->pixelformat = frame->pixelformat;
    clone->colorspace = frame->colorspace;
    clone->ncolors = frame->ncolors;
    clone->transparent = frame->transparent;
    clone->frame_no = frame->frame_no;
    clone->loop_count = frame->loop_count;
    clone->multiframe = frame->multiframe;
    clone->delay = frame->delay;

    if (frame->palette != NULL && frame->ncolors > 0) {
        if (frame->ncolors > SIXEL_PALETTE_MAX) {
            status = SIXEL_BAD_INPUT;
            goto error;
        }
        palette_bytes = frame->ncolors * 3;
        palette = (unsigned char *)sixel_allocator_malloc(
            clone->allocator,
            (size_t)palette_bytes);
        if (palette == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto error;
        }
        memcpy(palette, frame->palette, (size_t)palette_bytes);
        clone->palette = palette;
    }

    if (frame->width < 0 || frame->height < 0) {
        status = SIXEL_BAD_INPUT;
        goto error;
    }

    if (frame->width > 0 && frame->height > 0) {
        depth_result = sixel_helper_compute_depth(frame->pixelformat);
        if (depth_result <= 0) {
            status = SIXEL_BAD_INPUT;
            goto error;
        }
        depth = (size_t)depth_result;
        pixel_total = (size_t)frame->width * (size_t)frame->height;
        if (pixel_total / (size_t)frame->width
                != (size_t)frame->height) {
            status = SIXEL_BAD_INPUT;
            goto error;
        }
        if (pixel_total > SIZE_MAX / depth) {
            status = SIXEL_BAD_INPUT;
            goto error;
        }
        pixel_bytes = pixel_total * depth;
        if (pixel_bytes > 0U) {
            pixels = (unsigned char *)sixel_allocator_malloc(
                clone->allocator,
                pixel_bytes);
            if (pixels == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto error;
            }
            memcpy(pixels, sixel_frame_get_pixels(frame), pixel_bytes);
            clone->pixels.u8ptr = pixels;
        }
    }

    *frame_out = clone;
    return SIXEL_OK;

error:
    if (pixels != NULL) {
        sixel_allocator_free(clone->allocator, pixels);
        clone->pixels.u8ptr = NULL;
    }
    if (palette != NULL) {
        sixel_allocator_free(clone->allocator, palette);
        clone->palette = NULL;
    }
    sixel_frame_unref(clone);
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

    status = SIXEL_BAD_ARGUMENT;
    source_colorspace = SIXEL_COLORSPACE_GAMMA;
    pixelformat = SIXEL_PIXELFORMAT_RGB888;
    depth = 0;
    width = 0;
    height = 0;
    pixel_total = 0U;
    pixel_bytes = 0U;
    pixels = NULL;

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

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        pixels = (unsigned char *)sixel_frame_get_pixels_float32(frame);
    } else {
        pixels = sixel_frame_get_pixels(frame);
    }

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

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (source_colorspace == target_colorspace) {
        return SIXEL_OK;
    }

    palette_count = palette->entry_count;
    if (palette_count == 0U) {
        return SIXEL_OK;
    }

    palette_channels = palette_count * 3U;
    if (palette_channels / 3U != palette_count) {
        return SIXEL_BAD_INPUT;
    }

    if (palette->entries_float32 != NULL && palette->float_depth > 0) {
        palette_float_bytes =
            palette_count * (size_t)palette->float_depth;
        if ((size_t)palette->float_depth == 0U
                || palette_float_bytes / (size_t)palette->float_depth
                    != palette_count) {
            return SIXEL_BAD_INPUT;
        }
        source_pixelformat =
            sixel_encoder_pixelformat_for_colorspace(source_colorspace, 1);
        status = sixel_helper_convert_colorspace(
            (unsigned char *)palette->entries_float32,
            palette_float_bytes,
            source_pixelformat,
            source_colorspace,
            target_colorspace);
        if (SIXEL_FAILED(status)) {
            return status;
        }

        if (palette->entries == NULL) {
            return SIXEL_OK;
        }

        float_pixelformat =
            sixel_encoder_pixelformat_for_colorspace(target_colorspace, 1);
        for (index = 0U; index < palette_channels; ++index) {
            channel = (int)(index % 3U);
            palette->entries[index] =
                sixel_pixelformat_float_channel_to_byte(
                    float_pixelformat,
                    channel,
                    palette->entries_float32[index]);
        }

        return SIXEL_OK;
    }

    if (palette->entries == NULL) {
        return SIXEL_OK;
    }

    palette_bytes = palette_channels;
    status = sixel_helper_convert_colorspace(palette->entries,
                                             palette_bytes,
                                             SIXEL_PIXELFORMAT_RGB888,
                                             source_colorspace,
                                             target_colorspace);
    return status;
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


/* Compute raw byte size of one frame by pixelformat and geometry.
   Packed formats (1/2/4bpp) require ceil(width * bpp / 8) bytes per row. */
static size_t
sixel_encoder_compute_frame_size(
    int pixelformat,
    int width,
    int height)
{
    size_t size = 0;
    int bpp;
    int depth;

    if (width <= 0 || height <= 0) {
        goto end;
    }

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_G1:
        bpp = 1;
        break;
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_G2:
        bpp = 2;
        break;
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_G4:
        bpp = 4;
        break;
    default:
        depth = sixel_helper_compute_depth(pixelformat);
        if (depth <= 0) {
            goto end;
        }
        size = (size_t)width * (size_t)height * (size_t)depth;
        goto end;
    }

    size = (((size_t)width * (size_t)bpp + 7UL) / 8UL) * (size_t)height;

end:
    return size;
}


/* An clone function of XColorSpec() of xlib */
static SIXELSTATUS
sixel_parse_x_colorspec(
    unsigned char       /* out */ **bgcolor,     /* destination buffer */
    char const          /* in */  *s,            /* source buffer */
    sixel_allocator_t   /* in */  *allocator)    /* allocator object for
                                                    destination buffer */
{
    SIXELSTATUS status = SIXEL_FALSE;
    char *p;
    unsigned char components[3];
    int component_index = 0;
    unsigned long v;
    char *endptr;
    char *buf = NULL;
    struct color const *pcolor;
    size_t name_length;

    /* from rgb_lookup.h generated by gpref */
    name_length = strlen(s);
    if (name_length > (size_t)UINT_MAX) {
        sixel_helper_set_additional_message(
            "sixel_parse_x_colorspec: color name is too long.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    pcolor = lookup_rgb(s, (unsigned int)name_length);
    if (pcolor) {
        *bgcolor = (unsigned char *)sixel_allocator_malloc(allocator, 3);
        if (*bgcolor == NULL) {
            sixel_helper_set_additional_message(
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        (*bgcolor)[0] = pcolor->r;
        (*bgcolor)[1] = pcolor->g;
        (*bgcolor)[2] = pcolor->b;
    } else if (s[0] == 'r' && s[1] == 'g' && s[2] == 'b' && s[3] == ':') {
        p = buf = arg_strdup(s + 4, allocator);
        if (buf == NULL) {
            sixel_helper_set_additional_message(
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        while (*p) {
            v = 0;
            for (endptr = p; endptr - p <= 12; ++endptr) {
                if (*endptr >= '0' && *endptr <= '9') {
                    v = (v << 4) | (unsigned long)(*endptr - '0');
                } else if (*endptr >= 'a' && *endptr <= 'f') {
                    v = (v << 4) | (unsigned long)(*endptr - 'a' + 10);
                } else if (*endptr >= 'A' && *endptr <= 'F') {
                    v = (v << 4) | (unsigned long)(*endptr - 'A' + 10);
                } else {
                    break;
                }
            }
            if (endptr - p == 0) {
                break;
            }
            if (endptr - p > 4) {
                break;
            }
            v = v << ((4 - (endptr - p)) * 4) >> 8;
            components[component_index++] = (unsigned char)v;
            p = endptr;
            if (component_index == 3) {
                break;
            }
            if (*p == '\0') {
                break;
            }
            if (*p != '/') {
                break;
            }
            ++p;
        }
        if (component_index != 3 || *p != '\0' || *p == '/') {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        *bgcolor = (unsigned char *)sixel_allocator_malloc(allocator, 3);
        if (*bgcolor == NULL) {
            sixel_helper_set_additional_message(
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        (*bgcolor)[0] = components[0];
        (*bgcolor)[1] = components[1];
        (*bgcolor)[2] = components[2];
    } else if (*s == '#') {
        buf = arg_strdup(s + 1, allocator);
        if (buf == NULL) {
            sixel_helper_set_additional_message(
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (p = endptr = buf; endptr - p <= 12; ++endptr) {
            if (*endptr >= '0' && *endptr <= '9') {
                *endptr -= '0';
            } else if (*endptr >= 'a' && *endptr <= 'f') {
                *endptr -= 'a' - 10;
            } else if (*endptr >= 'A' && *endptr <= 'F') {
                *endptr -= 'A' - 10;
            } else if (*endptr == '\0') {
                break;
            } else {
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        if (endptr - p > 12) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        *bgcolor = (unsigned char *)sixel_allocator_malloc(allocator, 3);
        if (*bgcolor == NULL) {
            sixel_helper_set_additional_message(
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        switch (endptr - p) {
        case 3:
            (*bgcolor)[0] = (unsigned char)(p[0] << 4);
            (*bgcolor)[1] = (unsigned char)(p[1] << 4);
            (*bgcolor)[2] = (unsigned char)(p[2] << 4);
            break;
        case 6:
            (*bgcolor)[0] = (unsigned char)(p[0] << 4 | p[1]);
            (*bgcolor)[1] = (unsigned char)(p[2] << 4 | p[3]);
            (*bgcolor)[2] = (unsigned char)(p[4] << 4 | p[4]);
            break;
        case 9:
            (*bgcolor)[0] = (unsigned char)(p[0] << 4 | p[1]);
            (*bgcolor)[1] = (unsigned char)(p[3] << 4 | p[4]);
            (*bgcolor)[2] = (unsigned char)(p[6] << 4 | p[7]);
            break;
        case 12:
            (*bgcolor)[0] = (unsigned char)(p[0] << 4 | p[1]);
            (*bgcolor)[1] = (unsigned char)(p[4] << 4 | p[5]);
            (*bgcolor)[2] = (unsigned char)(p[8] << 4 | p[9]);
            break;
        default:
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
    } else {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, buf);

    return status;
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
                              sixel_logger_t *logger)
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


/* generic writer function for passing to sixel_output_new() */
static int
sixel_write_callback(char *data, int size, void *priv)
{
    int result;

    result = (int)sixel_compat_write(*(int *)priv,
                                     data,
                                     (size_t)size);

    return result;
}


/* the writer function with hex-encoding for passing to sixel_output_new() */
static int
sixel_hex_write_callback(
    char    /* in */ *data,
    int     /* in */ size,
    void    /* in */ *priv)
{
    char hex[SIXEL_OUTPUT_PACKET_SIZE * 2];
    int i;
    int j;
    int result;

    for (i = j = 0; i < size; ++i, ++j) {
        hex[j] = (data[i] >> 4) & 0xf;
        hex[j] += (hex[j] < 10 ? '0': ('a' - 10));
        hex[++j] = data[i] & 0xf;
        hex[j] += (hex[j] < 10 ? '0': ('a' - 10));
    }

    result = (int)sixel_compat_write(*(int *)priv,
                                     hex,
                                     (size_t)(size * 2));

    return result;
}

typedef struct sixel_encoder_output_probe {
    sixel_write_function base_write;
    void *base_priv;
} sixel_encoder_output_probe_t;

static int
sixel_write_with_probe(char *data, int size, void *priv)
{
    sixel_encoder_output_probe_t *probe;

    probe = (sixel_encoder_output_probe_t *)priv;
    if (probe == NULL || probe->base_write == NULL) {
        return 0;
    }
    return probe->base_write(data, size, probe->base_priv);
}

/*
 * Reuse the fn_write probe for raw escape writes so the output
 * path stays consistent across data and control sequences.
 *
 *     encoder        probe wrapper       write(2)
 *     +------+    +----------------+    +---------+
 *     | data | -> | sixel_write_*  | -> | target  |
 *     +------+    +----------------+    +---------+
 */
static int
sixel_encoder_probe_fd_write(sixel_encoder_t *encoder,
                             char *data,
                             int size,
                             int fd)
{
    sixel_encoder_output_probe_t probe;
    int written;

    (void)encoder;
    probe.base_write = sixel_write_callback;
    probe.base_priv = &fd;
    written = sixel_write_with_probe(data, size, &probe);

    return written;
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
    sixel_logger_t *logger;
    int job_id;
    int height;
    char message[256];
    va_list args;

    logger = NULL;
    if (encoder != NULL) {
        logger = encoder->logger;
    }
    if (logger == NULL || logger->file == NULL || !logger->active) {
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

    sixel_logger_logf(logger,
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
        paletted_pixels = sixel_dither_apply_palette(
            dither, (unsigned char *)pixels, width, height);
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
        unsigned char *palette_copy = NULL;
        size_t palette_count = 0U;

        status = sixel_dither_get_quantized_palette(dither, &palette_obj);
        if (SIXEL_SUCCEEDED(status) && palette_obj != NULL) {
            status = sixel_palette_copy_entries_8bit(
                palette_obj,
                &palette_copy,
                &palette_count,
                SIXEL_PIXELFORMAT_RGB888,
                encoder->allocator);
            sixel_palette_unref(palette_obj);
            palette_obj = NULL;
            if (SIXEL_SUCCEEDED(status)
                    && palette_copy != NULL
                    && palette_count > 0U) {
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
                        sixel_allocator_free(encoder->allocator,
                                             palette_copy);
                        goto cleanup;
                    }
                    sixel_allocator_free(encoder->allocator,
                                         encoder->capture_palette);
                    encoder->capture_palette = new_palette;
                    encoder->capture_palette_size = palette_bytes;
                }
                memcpy(encoder->capture_palette,
                       palette_copy,
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
            if (palette_copy != NULL) {
                sixel_allocator_free(encoder->allocator, palette_copy);
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


static int
sixel_path_has_extension(char const *path, char const *extension)
{
    size_t path_len;
    size_t ext_len;
    size_t index;

    path_len = 0u;
    ext_len = 0u;
    index = 0u;

    if (path == NULL || extension == NULL) {
        return 0;
    }

    path_len = strlen(path);
    ext_len = strlen(extension);
    if (ext_len == 0u || path_len < ext_len) {
        return 0;
    }

    for (index = 0u; index < ext_len; ++index) {
        unsigned char path_ch;
        unsigned char ext_ch;

        path_ch = (unsigned char)path[path_len - ext_len + index];
        ext_ch = (unsigned char)extension[index];
        if (tolower(path_ch) != tolower(ext_ch)) {
            return 0;
        }
    }

    return 1;
}

typedef enum sixel_palette_format {
    SIXEL_PALETTE_FORMAT_NONE = 0,
    SIXEL_PALETTE_FORMAT_ACT,
    SIXEL_PALETTE_FORMAT_PAL_JASC,
    SIXEL_PALETTE_FORMAT_PAL_RIFF,
    SIXEL_PALETTE_FORMAT_PAL_AUTO,
    SIXEL_PALETTE_FORMAT_GPL
} sixel_palette_format_t;

/*
 * Palette specification parser
 *
 *   TYPE:PATH  -> explicit format prefix
 *   PATH       -> rely on extension or heuristics
 *
 * The ASCII diagram below shows how the prefix is peeled:
 *
 *   [type] : [path]
 *    ^-- left part selects decoder/encoder when present.
 */
static char const *
sixel_palette_strip_prefix(char const *spec,
                           sixel_palette_format_t *format_hint)
{
    char const *colon;
    size_t type_len;
    size_t index;
    char lowered[16];

    colon = NULL;
    type_len = 0u;
    index = 0u;

    if (format_hint != NULL) {
        *format_hint = SIXEL_PALETTE_FORMAT_NONE;
    }
    if (spec == NULL) {
        return NULL;
    }

    colon = strchr(spec, ':');
    if (colon == NULL) {
        return spec;
    }

    type_len = (size_t)(colon - spec);
    if (type_len == 0u || type_len >= sizeof(lowered)) {
        return spec;
    }

    for (index = 0u; index < type_len; ++index) {
        lowered[index] = (char)tolower((unsigned char)spec[index]);
    }
    lowered[type_len] = '\0';

    if (strcmp(lowered, "act") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_ACT;
        }
        return colon + 1;
    }
    if (strcmp(lowered, "pal") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_PAL_AUTO;
        }
        return colon + 1;
    }
    if (strcmp(lowered, "pal-jasc") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_PAL_JASC;
        }
        return colon + 1;
    }
    if (strcmp(lowered, "pal-riff") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_PAL_RIFF;
        }
        return colon + 1;
    }
    if (strcmp(lowered, "gpl") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_GPL;
        }
        return colon + 1;
    }

    return spec;
}

static sixel_palette_format_t
sixel_palette_format_from_extension(char const *path)
{
    if (path == NULL) {
        return SIXEL_PALETTE_FORMAT_NONE;
    }

    if (sixel_path_has_extension(path, ".act")) {
        return SIXEL_PALETTE_FORMAT_ACT;
    }
    if (sixel_path_has_extension(path, ".pal")) {
        return SIXEL_PALETTE_FORMAT_PAL_AUTO;
    }
    if (sixel_path_has_extension(path, ".gpl")) {
        return SIXEL_PALETTE_FORMAT_GPL;
    }

    return SIXEL_PALETTE_FORMAT_NONE;
}

static int
sixel_path_has_any_extension(char const *path)
{
    char const *slash_forward;
#if defined(_WIN32)
    char const *slash_backward;
#endif
    char const *start;
    char const *dot;

    slash_forward = NULL;
#if defined(_WIN32)
    slash_backward = NULL;
#endif
    start = path;
    dot = NULL;

    if (path == NULL) {
        return 0;
    }

    slash_forward = strrchr(path, '/');
#if defined(_WIN32)
    slash_backward = strrchr(path, '\\');
    if (slash_backward != NULL &&
            (slash_forward == NULL || slash_backward > slash_forward)) {
        slash_forward = slash_backward;
    }
#endif
    if (slash_forward == NULL) {
        start = path;
    } else {
        start = slash_forward + 1;
    }

    dot = strrchr(start, '.');
    if (dot == NULL) {
        return 0;
    }

    if (dot[1] == '\0') {
        return 0;
    }

    return 1;
}

static int
sixel_palette_has_utf8_bom(unsigned char const *data, size_t size)
{
    if (data == NULL || size < 3u) {
        return 0;
    }
    if (data[0] == 0xefu && data[1] == 0xbbu && data[2] == 0xbfu) {
        return 1;
    }
    return 0;
}


/*
 * Materialize palette bytes from a stream.
 *
 * The flow looks like:
 *
 *   stream --> [scratch buffer] --> [resizable heap buffer]
 *                  ^ looped read        ^ returned payload
 */
static SIXELSTATUS
sixel_palette_read_stream(FILE *stream,
                          sixel_allocator_t *allocator,
                          unsigned char **pdata,
                          size_t *psize)
{
    SIXELSTATUS status;
    unsigned char *buffer;
    unsigned char *grown;
    size_t capacity;
    size_t used;
    size_t read_bytes;
    size_t needed;
    size_t new_capacity;
    unsigned char scratch[4096];

    status = SIXEL_FALSE;
    buffer = NULL;
    grown = NULL;
    capacity = 0u;
    used = 0u;
    read_bytes = 0u;
    needed = 0u;
    new_capacity = 0u;

    if (pdata == NULL || psize == NULL || stream == NULL || allocator == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_read_stream: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    *pdata = NULL;
    *psize = 0u;

    while (1) {
        read_bytes = fread(scratch, 1, sizeof(scratch), stream);
        if (read_bytes == 0u) {
            if (ferror(stream)) {
                sixel_helper_set_additional_message(
                    "sixel_palette_read_stream: fread() failed.");
                status = SIXEL_LIBC_ERROR;
                goto cleanup;
            }
            break;
        }

        if (used > SIZE_MAX - read_bytes) {
            sixel_helper_set_additional_message(
                "sixel_palette_read_stream: size overflow.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        needed = used + read_bytes;

        if (needed > capacity) {
            new_capacity = capacity;
            if (new_capacity == 0u) {
                new_capacity = 4096u;
            }
            while (needed > new_capacity) {
                if (new_capacity > SIZE_MAX / 2u) {
                    sixel_helper_set_additional_message(
                        "sixel_palette_read_stream: size overflow.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                new_capacity *= 2u;
            }

            grown = (unsigned char *)sixel_allocator_malloc(allocator,
                                                             new_capacity);
            if (grown == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_palette_read_stream: allocation failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto cleanup;
            }

            if (buffer != NULL) {
                memcpy(grown, buffer, used);
                sixel_allocator_free(allocator, buffer);
            }

            buffer = grown;
            grown = NULL;
            capacity = new_capacity;
        }

        memcpy(buffer + used, scratch, read_bytes);
        used += read_bytes;
    }

    *pdata = buffer;
    *psize = used;
    status = SIXEL_OK;
    return status;

cleanup:
    if (grown != NULL) {
        sixel_allocator_free(allocator, grown);
    }
    if (buffer != NULL) {
        sixel_allocator_free(allocator, buffer);
    }
    return status;
}


static SIXELSTATUS
sixel_palette_open_read(char const *path, FILE **pstream, int *pclose)
{
    int error_value;
    char error_message[256];
    char strerror_buffer[128];
#if HAVE_SYS_STAT_H
    struct stat path_stat;
#endif

    if (pstream == NULL || pclose == NULL || path == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_open_read: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    error_value = 0;
    error_message[0] = '\0';

    if (strcmp(path, "-") == 0) {
        *pstream = stdin;
        *pclose = 0;
        return SIXEL_OK;
    }

#if HAVE_SYS_STAT_H
    if (stat(path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        sixel_compat_snprintf(error_message,
                              sizeof(error_message),
                              "sixel_palette_open_read: mapfile \"%s\" "
                              "is a directory.",
                              path);
        sixel_helper_set_additional_message(error_message);
        return SIXEL_BAD_INPUT;
    }
#endif

    errno = 0;
    *pstream = sixel_compat_fopen(path, "rb");
    if (*pstream == NULL) {
        error_value = errno;
        sixel_compat_snprintf(error_message,
                              sizeof(error_message),
                              "sixel_palette_open_read: failed to open "
                              "\"%s\": %s.",
                              path,
                              sixel_compat_strerror(error_value,
                                                    strerror_buffer,
                                                    sizeof(strerror_buffer)));
        sixel_helper_set_additional_message(error_message);
        return SIXEL_LIBC_ERROR;
    }

    *pclose = 1;
    return SIXEL_OK;
}


static void
sixel_palette_close_stream(FILE *stream, int close_stream)
{
    if (close_stream && stream != NULL) {
        (void) fclose(stream);
    }
}


static sixel_palette_format_t
sixel_palette_guess_format(unsigned char const *data, size_t size)
{
    size_t offset;
    size_t data_size;

    offset = 0u;
    data_size = size;

    if (data == NULL || size == 0u) {
        return SIXEL_PALETTE_FORMAT_NONE;
    }

    if (size == 256u * 3u || size == 256u * 3u + 4u) {
        return SIXEL_PALETTE_FORMAT_ACT;
    }

    if (size >= 12u && memcmp(data, "RIFF", 4) == 0
            && memcmp(data + 8, "PAL ", 4) == 0) {
        return SIXEL_PALETTE_FORMAT_PAL_RIFF;
    }

    if (sixel_palette_has_utf8_bom(data, size)) {
        offset = 3u;
        data_size = size - 3u;
    }

    if (data_size >= 8u && memcmp(data + offset, "JASC-PAL", 8) == 0) {
        return SIXEL_PALETTE_FORMAT_PAL_JASC;
    }
    if (data_size >= 12u && memcmp(data + offset, "GIMP Palette", 12) == 0) {
        return SIXEL_PALETTE_FORMAT_GPL;
    }

    return SIXEL_PALETTE_FORMAT_NONE;
}


static unsigned int
sixel_palette_read_le16(unsigned char const *ptr)
{
    if (ptr == NULL) {
        return 0u;
    }
    return (unsigned int)ptr[0] | ((unsigned int)ptr[1] << 8);
}


static unsigned int
sixel_palette_read_le32(unsigned char const *ptr)
{
    if (ptr == NULL) {
        return 0u;
    }
    return ((unsigned int)ptr[0])
        | ((unsigned int)ptr[1] << 8)
        | ((unsigned int)ptr[2] << 16)
        | ((unsigned int)ptr[3] << 24);
}


/*
 * Adobe Color Table (*.act) reader
 *
 *   +-----------+---------------------------+
 *   | section   | bytes                     |
 *   +-----------+---------------------------+
 *   | palette   | 256 entries * 3 RGB bytes |
 *   | trailer   | optional count/start pair |
 *   +-----------+---------------------------+
 */
static SIXELSTATUS
sixel_palette_parse_act(unsigned char const *data,
                        size_t size,
                        sixel_encoder_t *encoder,
                        sixel_dither_t **dither)
{
    SIXELSTATUS status;
    sixel_dither_t *local;
    unsigned char const *palette_start;
    unsigned char const *trailer;
    sixel_palette_t *palette_obj;
    int exported_colors;
    int start_index;

    status = SIXEL_FALSE;
    local = NULL;
    palette_start = data;
    trailer = NULL;
    palette_obj = NULL;
    exported_colors = 0;
    start_index = 0;

    if (encoder == NULL || dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (data == NULL || size < 256u * 3u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: truncated ACT palette.");
        return SIXEL_BAD_INPUT;
    }

    if (size == 256u * 3u) {
        exported_colors = 256;
        start_index = 0;
    } else if (size == 256u * 3u + 4u) {
        trailer = data + 256u * 3u;
        exported_colors = (int)(((unsigned int)trailer[0] << 8)
                                | (unsigned int)trailer[1]);
        start_index = (int)(((unsigned int)trailer[2] << 8)
                            | (unsigned int)trailer[3]);
    } else {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: invalid ACT length.");
        return SIXEL_BAD_INPUT;
    }

    if (start_index < 0 || start_index >= 256) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: ACT start index out of range.");
        return SIXEL_BAD_INPUT;
    }
    if (exported_colors <= 0 || exported_colors > 256) {
        exported_colors = 256;
    }
    if (start_index + exported_colors > 256) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: ACT palette exceeds 256 slots.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_dither_new(&local, exported_colors, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    sixel_dither_set_lut_policy(local, encoder->lut_policy);

    status = sixel_dither_get_quantized_palette(local, &palette_obj);
    if (SIXEL_FAILED(status) || palette_obj == NULL) {
        sixel_dither_unref(local);
        return status;
    }
    status = sixel_palette_set_entries(
        palette_obj,
        palette_start + (size_t)start_index * 3u,
        (unsigned int)exported_colors,
        3,
        encoder->allocator);
    sixel_palette_unref(palette_obj);
    if (SIXEL_FAILED(status)) {
        sixel_dither_unref(local);
        return status;
    }

    *dither = local;
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_palette_parse_pal_jasc(unsigned char const *data,
                             size_t size,
                             sixel_encoder_t *encoder,
                             sixel_dither_t **dither)
{
    SIXELSTATUS status;
    char *text;
    size_t index;
    size_t offset;
    char *cursor;
    char *line;
    char *line_end;
    int stage;
    int exported_colors;
    int parsed_colors;
    sixel_dither_t *local;
    sixel_palette_t *palette_obj;
    unsigned char *palette_buffer;
    long component;
    char *parse_end;
    int value_index;
    int values[3];
    char tail;

    status = SIXEL_FALSE;
    text = NULL;
    index = 0u;
    offset = 0u;
    cursor = NULL;
    line = NULL;
    line_end = NULL;
    stage = 0;
    exported_colors = 0;
    parsed_colors = 0;
    local = NULL;
    palette_obj = NULL;
    palette_buffer = NULL;
    component = 0;
    parse_end = NULL;
    value_index = 0;
    values[0] = 0;
    values[1] = 0;
    values[2] = 0;

    if (encoder == NULL || dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (data == NULL || size == 0u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: empty palette.");
        return SIXEL_BAD_INPUT;
    }

    text = (char *)sixel_allocator_malloc(encoder->allocator, size + 1u);
    if (text == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(text, data, size);
    text[size] = '\0';

    if (sixel_palette_has_utf8_bom((unsigned char const *)text, size)) {
        offset = 3u;
    }
    cursor = text + offset;

    while (*cursor != '\0') {
        line = cursor;
        line_end = cursor;
        while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
            ++line_end;
        }
        if (*line_end != '\0') {
            *line_end = '\0';
            cursor = line_end + 1;
        } else {
            cursor = line_end;
        }
        while (*cursor == '\n' || *cursor == '\r') {
            ++cursor;
        }

        while (*line == ' ' || *line == '\t') {
            ++line;
        }
        index = strlen(line);
        while (index > 0u) {
            tail = line[index - 1];
            if (tail != ' ' && tail != '\t') {
                break;
            }
            line[index - 1] = '\0';
            --index;
        }
        if (*line == '\0') {
            continue;
        }
        if (*line == '#') {
            continue;
        }

        if (stage == 0) {
            if (strcmp(line, "JASC-PAL") != 0) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: missing header.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            stage = 1;
            continue;
        }
        if (stage == 1) {
            stage = 2;
            continue;
        }
        if (stage == 2) {
            component = strtol(line, &parse_end, 10);
            if (parse_end == line || component <= 0L || component > 256L) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: invalid color count.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            exported_colors = (int)component;
            if (exported_colors <= 0) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: invalid color count.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            palette_buffer = (unsigned char *)sixel_allocator_malloc(
                encoder->allocator,
                (size_t)exported_colors * 3u);
            if (palette_buffer == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: allocation failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto cleanup;
            }
            status = sixel_dither_new(&local, exported_colors,
                                      encoder->allocator);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            sixel_dither_set_lut_policy(local, encoder->lut_policy);
            stage = 3;
            continue;
        }

        value_index = 0;
        while (value_index < 3) {
            component = strtol(line, &parse_end, 10);
            if (parse_end == line || component < 0L || component > 255L) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: invalid component.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            values[value_index] = (int)component;
            ++value_index;
            line = parse_end;
            while (*line == ' ' || *line == '\t') {
                ++line;
            }
        }

        if (parsed_colors >= exported_colors) {
            sixel_helper_set_additional_message(
                "sixel_palette_parse_pal_jasc: excess entries.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }

        palette_buffer[parsed_colors * 3 + 0] =
            (unsigned char)values[0];
        palette_buffer[parsed_colors * 3 + 1] =
            (unsigned char)values[1];
        palette_buffer[parsed_colors * 3 + 2] =
            (unsigned char)values[2];
        ++parsed_colors;
    }

    if (stage < 3) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: incomplete header.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    if (parsed_colors != exported_colors) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: color count mismatch.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    status = sixel_dither_get_quantized_palette(local, &palette_obj);
    if (SIXEL_FAILED(status) || palette_obj == NULL) {
        goto cleanup;
    }
    status = sixel_palette_set_entries(palette_obj,
                                       palette_buffer,
                                       (unsigned int)exported_colors,
                                       3,
                                       encoder->allocator);
    sixel_palette_unref(palette_obj);
    palette_obj = NULL;
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    *dither = local;
    status = SIXEL_OK;

cleanup:
    if (palette_obj != NULL) {
        sixel_palette_unref(palette_obj);
    }
    if (SIXEL_FAILED(status) && local != NULL) {
        sixel_dither_unref(local);
    }
    if (palette_buffer != NULL) {
        sixel_allocator_free(encoder->allocator, palette_buffer);
    }
    if (text != NULL) {
        sixel_allocator_free(encoder->allocator, text);
    }
    return status;
}


static SIXELSTATUS
sixel_palette_parse_pal_riff(unsigned char const *data,
                             size_t size,
                             sixel_encoder_t *encoder,
                             sixel_dither_t **dither)
{
    SIXELSTATUS status;
    size_t offset;
    size_t chunk_size;
    sixel_dither_t *local;
    sixel_palette_t *palette_obj;
    unsigned char const *chunk;
    unsigned char *palette_buffer;
    unsigned int entry_count;
    unsigned int version;
    unsigned int index;
    size_t palette_offset;

    status = SIXEL_FALSE;
    offset = 0u;
    chunk_size = 0u;
    local = NULL;
    chunk = NULL;
    palette_obj = NULL;
    palette_buffer = NULL;
    entry_count = 0u;
    version = 0u;
    index = 0u;
    palette_offset = 0u;

    if (encoder == NULL || dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (data == NULL || size < 12u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: truncated palette.");
        return SIXEL_BAD_INPUT;
    }
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "PAL ", 4) != 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: missing RIFF header.");
        return SIXEL_BAD_INPUT;
    }

    offset = 12u;
    while (offset + 8u <= size) {
        chunk = data + offset;
        chunk_size = (size_t)sixel_palette_read_le32(chunk + 4);
        if (offset + 8u + chunk_size > size) {
            sixel_helper_set_additional_message(
                "sixel_palette_parse_pal_riff: chunk extends past end.");
            return SIXEL_BAD_INPUT;
        }
        if (memcmp(chunk, "data", 4) == 0) {
            break;
        }
        offset += 8u + ((chunk_size + 1u) & ~1u);
    }

    if (offset + 8u > size || memcmp(chunk, "data", 4) != 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: missing data chunk.");
        return SIXEL_BAD_INPUT;
    }

    if (chunk_size < 4u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: data chunk too small.");
        return SIXEL_BAD_INPUT;
    }
    version = sixel_palette_read_le16(chunk + 8);
    (void)version;
    entry_count = sixel_palette_read_le16(chunk + 10);
    if (entry_count == 0u || entry_count > 256u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: invalid entry count.");
        return SIXEL_BAD_INPUT;
    }
    if (chunk_size != 4u + (size_t)entry_count * 4u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: unexpected chunk size.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_dither_new(&local, (int)entry_count, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    sixel_dither_set_lut_policy(local, encoder->lut_policy);
    palette_buffer = (unsigned char *)sixel_allocator_malloc(
        encoder->allocator,
        (size_t)entry_count * 3u);
    if (palette_buffer == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: allocation failed.");
        sixel_dither_unref(local);
        return SIXEL_BAD_ALLOCATION;
    }
    palette_offset = 12u;
    for (index = 0u; index < entry_count; ++index) {
        palette_buffer[index * 3u + 0u] =
            chunk[palette_offset + index * 4u + 0u];
        palette_buffer[index * 3u + 1u] =
            chunk[palette_offset + index * 4u + 1u];
        palette_buffer[index * 3u + 2u] =
            chunk[palette_offset + index * 4u + 2u];
    }

    status = sixel_dither_get_quantized_palette(local, &palette_obj);
    if (SIXEL_FAILED(status) || palette_obj == NULL) {
        sixel_allocator_free(encoder->allocator, palette_buffer);
        sixel_dither_unref(local);
        return status;
    }
    status = sixel_palette_set_entries(palette_obj,
                                       palette_buffer,
                                       (unsigned int)entry_count,
                                       3,
                                       encoder->allocator);
    sixel_palette_unref(palette_obj);
    palette_obj = NULL;
    sixel_allocator_free(encoder->allocator, palette_buffer);
    palette_buffer = NULL;
    if (SIXEL_FAILED(status)) {
        sixel_dither_unref(local);
        return status;
    }

    *dither = local;
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_palette_parse_gpl(unsigned char const *data,
                        size_t size,
                        sixel_encoder_t *encoder,
                        sixel_dither_t **dither)
{
    SIXELSTATUS status;
    char *text;
    size_t offset;
    char *cursor;
    char *line;
    char *line_end;
    size_t index;
    int header_seen;
    int parsed_colors;
    unsigned char palette_bytes[256 * 3];
    long component;
    char *parse_end;
    int value_index;
    int values[3];
    sixel_dither_t *local;
    sixel_palette_t *palette_obj;
    char tail;

    status = SIXEL_FALSE;
    text = NULL;
    offset = 0u;
    cursor = NULL;
    line = NULL;
    line_end = NULL;
    index = 0u;
    header_seen = 0;
    parsed_colors = 0;
    component = 0;
    parse_end = NULL;
    value_index = 0;
    values[0] = 0;
    values[1] = 0;
    values[2] = 0;
    local = NULL;
    palette_obj = NULL;

    if (encoder == NULL || dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (data == NULL || size == 0u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: empty palette.");
        return SIXEL_BAD_INPUT;
    }

    text = (char *)sixel_allocator_malloc(encoder->allocator, size + 1u);
    if (text == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(text, data, size);
    text[size] = '\0';

    if (sixel_palette_has_utf8_bom((unsigned char const *)text, size)) {
        offset = 3u;
    }
    cursor = text + offset;

    while (*cursor != '\0') {
        line = cursor;
        line_end = cursor;
        while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
            ++line_end;
        }
        if (*line_end != '\0') {
            *line_end = '\0';
            cursor = line_end + 1;
        } else {
            cursor = line_end;
        }
        while (*cursor == '\n' || *cursor == '\r') {
            ++cursor;
        }

        while (*line == ' ' || *line == '\t') {
            ++line;
        }
        index = strlen(line);
        while (index > 0u) {
            tail = line[index - 1];
            if (tail != ' ' && tail != '\t') {
                break;
            }
            line[index - 1] = '\0';
            --index;
        }
        if (*line == '\0') {
            continue;
        }
        if (*line == '#') {
            continue;
        }
        if (strncmp(line, "Name:", 5) == 0) {
            continue;
        }
        if (strncmp(line, "Columns:", 8) == 0) {
            continue;
        }

        if (!header_seen) {
            if (strcmp(line, "GIMP Palette") != 0) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_gpl: missing header.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            header_seen = 1;
            continue;
        }

        if (parsed_colors >= 256) {
            sixel_helper_set_additional_message(
                "sixel_palette_parse_gpl: too many colors.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }

        value_index = 0;
        while (value_index < 3) {
            component = strtol(line, &parse_end, 10);
            if (parse_end == line || component < 0L || component > 255L) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_gpl: invalid component.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            values[value_index] = (int)component;
            ++value_index;
            line = parse_end;
            while (*line == ' ' || *line == '\t') {
                ++line;
            }
        }

        palette_bytes[parsed_colors * 3 + 0] =
            (unsigned char)values[0];
        palette_bytes[parsed_colors * 3 + 1] =
            (unsigned char)values[1];
        palette_bytes[parsed_colors * 3 + 2] =
            (unsigned char)values[2];
        ++parsed_colors;
    }

    if (!header_seen) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: header missing.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    if (parsed_colors <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: no colors parsed.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    status = sixel_dither_new(&local, parsed_colors, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    sixel_dither_set_lut_policy(local, encoder->lut_policy);
    status = sixel_dither_get_quantized_palette(local, &palette_obj);
    if (SIXEL_FAILED(status) || palette_obj == NULL) {
        goto cleanup;
    }
    status = sixel_palette_set_entries(palette_obj,
                                       palette_bytes,
                                       (unsigned int)parsed_colors,
                                       3,
                                       encoder->allocator);
    sixel_palette_unref(palette_obj);
    palette_obj = NULL;
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    *dither = local;
    status = SIXEL_OK;

cleanup:
    if (palette_obj != NULL) {
        sixel_palette_unref(palette_obj);
    }
    if (SIXEL_FAILED(status) && local != NULL) {
        sixel_dither_unref(local);
    }
    if (text != NULL) {
        sixel_allocator_free(encoder->allocator, text);
    }
    return status;
}


/*
 * Palette exporters
 *
 *   +----------+-------------------------+
 *   | format   | emission strategy       |
 *   +----------+-------------------------+
 *   | ACT      | fixed 256 entries + EOF |
 *   | PAL JASC | textual lines           |
 *   | PAL RIFF | RIFF container          |
 *   | GPL      | textual lines           |
 *   +----------+-------------------------+
 */
static SIXELSTATUS
sixel_palette_write_act(FILE *stream,
                        unsigned char const *palette,
                        int exported_colors)
{
    SIXELSTATUS status;
    unsigned char act_table[256 * 3];
    unsigned char trailer[4];
    size_t exported_bytes;

    status = SIXEL_FALSE;
    exported_bytes = 0u;

    if (stream == NULL || palette == NULL || exported_colors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (exported_colors > 256) {
        exported_colors = 256;
    }

    memset(act_table, 0, sizeof(act_table));
    exported_bytes = (size_t)exported_colors * 3u;
    memcpy(act_table, palette, exported_bytes);

    trailer[0] = (unsigned char)(((unsigned int)exported_colors >> 8)
                                 & 0xffu);
    trailer[1] = (unsigned char)((unsigned int)exported_colors & 0xffu);
    trailer[2] = 0u;
    trailer[3] = 0u;

    if (fwrite(act_table, 1, sizeof(act_table), stream)
            != sizeof(act_table)) {
        status = SIXEL_LIBC_ERROR;
        return status;
    }
    if (fwrite(trailer, 1, sizeof(trailer), stream)
            != sizeof(trailer)) {
        status = SIXEL_LIBC_ERROR;
        return status;
    }

    return SIXEL_OK;
}


static SIXELSTATUS
sixel_palette_write_pal_jasc(FILE *stream,
                             unsigned char const *palette,
                             int exported_colors)
{
    int index;

    if (stream == NULL || palette == NULL || exported_colors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (fprintf(stream, "JASC-PAL\n0100\n%d\n", exported_colors) < 0) {
        return SIXEL_LIBC_ERROR;
    }
    for (index = 0; index < exported_colors; ++index) {
        if (fprintf(stream, "%d %d %d\n",
                    (int)palette[index * 3 + 0],
                    (int)palette[index * 3 + 1],
                    (int)palette[index * 3 + 2]) < 0) {
            return SIXEL_LIBC_ERROR;
        }
    }
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_palette_write_pal_riff(FILE *stream,
                             unsigned char const *palette,
                             int exported_colors)
{
    unsigned char header[12];
    unsigned char chunk[8];
    unsigned char log_palette[4 + 256 * 4];
    unsigned int data_size;
    unsigned int riff_size;
    int index;

    if (stream == NULL || palette == NULL || exported_colors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (exported_colors > 256) {
        exported_colors = 256;
    }

    data_size = 4u + (unsigned int)exported_colors * 4u;
    riff_size = 4u + 8u + data_size;

    memcpy(header, "RIFF", 4);
    header[4] = (unsigned char)(riff_size & 0xffu);
    header[5] = (unsigned char)((riff_size >> 8) & 0xffu);
    header[6] = (unsigned char)((riff_size >> 16) & 0xffu);
    header[7] = (unsigned char)((riff_size >> 24) & 0xffu);
    memcpy(header + 8, "PAL ", 4);

    memcpy(chunk, "data", 4);
    chunk[4] = (unsigned char)(data_size & 0xffu);
    chunk[5] = (unsigned char)((data_size >> 8) & 0xffu);
    chunk[6] = (unsigned char)((data_size >> 16) & 0xffu);
    chunk[7] = (unsigned char)((data_size >> 24) & 0xffu);

    memset(log_palette, 0, sizeof(log_palette));
    log_palette[0] = 0x00;
    log_palette[1] = 0x03;
    log_palette[2] = (unsigned char)(exported_colors & 0xff);
    log_palette[3] = (unsigned char)((exported_colors >> 8) & 0xff);
    for (index = 0; index < exported_colors; ++index) {
        log_palette[4 + index * 4 + 0] = palette[index * 3 + 0];
        log_palette[4 + index * 4 + 1] = palette[index * 3 + 1];
        log_palette[4 + index * 4 + 2] = palette[index * 3 + 2];
        log_palette[4 + index * 4 + 3] = 0u;
    }

    if (fwrite(header, 1, sizeof(header), stream)
            != sizeof(header)) {
        return SIXEL_LIBC_ERROR;
    }
    if (fwrite(chunk, 1, sizeof(chunk), stream) != sizeof(chunk)) {
        return SIXEL_LIBC_ERROR;
    }
    if (fwrite(log_palette, 1, (size_t)data_size, stream)
            != (size_t)data_size) {
        return SIXEL_LIBC_ERROR;
    }
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_palette_write_gpl(FILE *stream,
                        unsigned char const *palette,
                        int exported_colors)
{
    int index;

    if (stream == NULL || palette == NULL || exported_colors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (fprintf(stream, "GIMP Palette\n") < 0) {
        return SIXEL_LIBC_ERROR;
    }
    if (fprintf(stream, "Name: libsixel export\n") < 0) {
        return SIXEL_LIBC_ERROR;
    }
    if (fprintf(stream, "Columns: 16\n") < 0) {
        return SIXEL_LIBC_ERROR;
    }
    if (fprintf(stream, "# Exported by libsixel\n") < 0) {
        return SIXEL_LIBC_ERROR;
    }
    for (index = 0; index < exported_colors; ++index) {
        if (fprintf(stream, "%3d %3d %3d\tIndex %d\n",
                    (int)palette[index * 3 + 0],
                    (int)palette[index * 3 + 1],
                    (int)palette[index * 3 + 2],
                    index) < 0) {
            return SIXEL_LIBC_ERROR;
        }
    }
    return SIXEL_OK;
}


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


typedef struct sixel_encode_dag_context {
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    SIXELSTATUS status;
    sixel_dither_t *dither;
    int height;
    int is_animation;
    int nwrite;
    int drcs_is_96cs_param;
    char drcs_designate_str[4];
    char buf[256];
    sixel_write_function fn_write;
    sixel_write_function write_callback;
    sixel_write_function scroll_callback;
    void *write_priv;
    void *scroll_priv;
    sixel_encoder_output_probe_t probe;
    sixel_encoder_output_probe_t scroll_probe;
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
    sixel_filter_dither_config_t dither_config;
    int current_pixelformat;
    int current_colorspace;
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

    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

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

    status = sixel_encoder_apply_lut_filter(context->encoder,
                                            context->dither);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (context->encoder->dither_cache != NULL) {
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

    sixel_dither_set_diffusion_type(context->dither,
                                    context->encoder->method_for_diffuse);
    sixel_dither_set_diffusion_scan(context->dither,
                                    context->encoder->method_for_scan);
    sixel_dither_set_diffusion_carry(context->dither,
                                     context->encoder->method_for_carry);

    if (context->encoder->complexion > 1) {
        sixel_dither_set_complexion_score(context->dither,
                                          context->encoder->complexion);
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encode_dag_node_dither_plan(sixel_encode_dag_context_t *context)
{
    SIXELSTATUS status;
    int height;

    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    context->dither_config.dither = context->dither;
    height = sixel_frame_get_height(context->frame);
    if (height < 0) {
        height = 0;
    }

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
    int drcs_is_96cs_param;
    int nwrite;

    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = SIXEL_OK;

    if (context->output) {
        sixel_output_ref(context->output);
        context->fn_write = context->output->fn_write;
        context->write_callback = context->output->fn_write;
        context->write_priv = context->output->priv;
    } else {
        if (context->encoder->fuse_macro
            || context->encoder->macro_number >= 0) {
            context->fn_write = sixel_hex_write_callback;
        } else {
            context->fn_write = sixel_write_callback;
        }
        context->write_callback = context->fn_write;
        context->write_priv = &context->encoder->outfd;
        status = sixel_output_new(&context->output,
                                  context->write_callback,
                                  context->write_priv,
                                  context->encoder->allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
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

    if (sixel_frame_get_multiframe(context->frame)
        && !context->encoder->fstatic) {
        if (sixel_frame_get_loop_no(context->frame) != 0
            || sixel_frame_get_frame_no(context->frame) != 0) {
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

    if (context->encoder->cancel_flag && *context->encoder->cancel_flag) {
        return SIXEL_INTERRUPTED;
    }

    if (context->encoder->fdrcs) {
        if (context->encoder->drcs_mmv == 0) {
            drcs_is_96cs_param =
                (context->encoder->drcs_charset_no > 63u) ? 1 : 0;
            context->drcs_designate_str[1] =
                (char)(((context->encoder->drcs_charset_no - 1u) % 63u)
                       + 0x40u);
            context->drcs_designate_str[2] = 0x00;
        } else if (context->encoder->drcs_mmv == 1) {
            drcs_is_96cs_param = 0;
            context->drcs_designate_str[1] =
                (char)(context->encoder->drcs_charset_no + 0x3fu);
            context->drcs_designate_str[2] = 0x00;
        } else if (context->encoder->drcs_mmv == 2) {
            drcs_is_96cs_param =
                (context->encoder->drcs_charset_no > 79u) ? 1 : 0;
            context->drcs_designate_str[1] =
                (char)(((context->encoder->drcs_charset_no - 1u) % 79u)
                       + 0x30u);
            context->drcs_designate_str[2] = 0x00;
        } else {
            drcs_is_96cs_param = 0;
            context->drcs_designate_str[1] =
                (char)(((context->encoder->drcs_charset_no - 1u) / 63u)
                       + 0x20u);
            context->drcs_designate_str[2] =
                (char)(((context->encoder->drcs_charset_no - 1u) % 63u)
                       + 0x40u);
            context->drcs_designate_str[3] = 0x00;
        }
        context->drcs_is_96cs_param = drcs_is_96cs_param;
        nwrite = sixel_compat_snprintf(
            context->buf,
            sizeof(context->buf),
            "%s%sh%s1;0;0;%d;1;3;%d;%d{%s",
            (context->encoder->drcs_mmv > 0) ? (
                context->encoder->f8bit ? "\233?8800": "\033[?8800"
            ): "",
            (context->encoder->drcs_mmv >= 3) ? (
                context->encoder->f8bit ? ";8801": ";8801"
            ): "",
            context->encoder->f8bit ? "\220": "\033P",
            context->encoder->cell_width,
            context->encoder->cell_height,
            drcs_is_96cs_param,
            context->drcs_designate_str);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: command format failed.");
            return status;
        }
        nwrite = context->write_callback(context->buf, nwrite,
                                         context->write_priv);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: write() failed.");
            return status;
        }
    }

    if (context->encoder->fuse_macro) {
        status = sixel_encoder_output_with_macro(context->frame,
                                                 context->dither,
                                                 context->output,
                                                 context->encoder);
    } else if (context->encoder->macro_number >= 0) {
        status = sixel_encoder_output_with_macro(context->frame,
                                                 context->dither,
                                                 context->output,
                                                 context->encoder);
    } else {
        status = sixel_encoder_output_without_macro(context->frame,
                                                    context->dither,
                                                    context->output,
                                                    context->encoder);
    }
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (context->encoder->cancel_flag && *context->encoder->cancel_flag) {
        nwrite = context->write_callback("\x18\033\\", 3,
                                         context->write_priv);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: write_callback() failed.");
            return status;
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
            if (context->encoder->drcs_mmv == 0) {
                status = sixel_encoder_emit_iso2022_chars(context->encoder,
                                                          context->frame);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
            } else {
                status = sixel_encoder_emit_drcsmmv2_chars(context->encoder,
                                                          context->frame);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
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

    job = (sixel_palette_async_job_t *)priv;
    status = SIXEL_BAD_ARGUMENT;
    local = NULL;

    if (job != NULL && job->encoder != NULL && job->sample_frame != NULL) {
        status = sixel_frame_set_pixelformat(job->sample_frame,
                                             job->target_pixelformat);
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
    callback_context.prefer_float32 = encoder->prefer_float32;

    sixel_helper_set_loader_trace(encoder->verbose);
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


/* create dither object from a frame */
static SIXELSTATUS
sixel_encoder_prepare_palette(
    sixel_encoder_t *encoder,  /* encoder object */
    sixel_frame_t   *frame,    /* input frame object */
    sixel_dither_t  **dither,  /* dither object to be created from the frame */
    int allow_cache,
    sixel_logger_t *logger)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int histogram_colors;
    sixel_filter_final_merge_config_t merge_config;
    sixel_logger_t *target_logger;
    int cache_allowed;
    sixel_frame_t *palette_frame;
    sixel_frame_t *cluster_frame;
    unsigned char *palette_pixels;
    int palette_pixelformat;
    int palette_target_pixelformat;
    int clustering_colorspace;
    int working_colorspace;
    int prefer_float32;

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
    if (encoder != NULL) {
        if (target_logger == NULL) {
            target_logger = encoder->logger;
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
        if (cache_allowed && encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_monochrome_palette(dither, encoder->finvert);
        }
        goto end;
    case SIXEL_COLOR_OPTION_MAPFILE:
        if (cache_allowed && encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_specified_palette(dither, encoder);
        }
        goto end;
    case SIXEL_COLOR_OPTION_BUILTIN:
        if (cache_allowed && encoder->dither_cache) {
            *dither = encoder->dither_cache;
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
    status = sixel_dither_new(dither, encoder->reqcolors, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    clustering_colorspace = encoder->clustering_colorspace;
    working_colorspace = encoder->working_colorspace;
    prefer_float32 = encoder->prefer_float32;
    palette_target_pixelformat =
        sixel_encoder_pixelformat_for_colorspace(clustering_colorspace,
                                                 prefer_float32);

    if (sixel_frame_get_pixelformat(frame) != palette_target_pixelformat
            || sixel_frame_get_colorspace(frame)
                != clustering_colorspace) {
        status = sixel_encoder_clone_frame(frame,
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

    sixel_dither_set_lut_policy(*dither, encoder->lut_policy);
    sixel_dither_set_sixel_reversible(*dither,
                                      encoder->sixel_reversible);
    memset(&merge_config, 0, sizeof(merge_config));
    merge_config.dither = *dither;
    merge_config.final_merge_mode = encoder->final_merge_mode;
    status = sixel_filter_final_merge_apply(&merge_config, target_logger);
    if (SIXEL_FAILED(status)) {
        sixel_dither_unref(*dither);
        goto end;
    }
    (*dither)->quantize_model = encoder->quantize_model;

    palette_pixels = sixel_frame_get_pixels(palette_frame);
    palette_pixelformat = sixel_frame_get_pixelformat(palette_frame);
    sixel_set_kmeans_init_type_override(
        encoder->quantize_model_kmeans_init_override,
        (sixel_kmeans_init_type)encoder->quantize_model_kmeans_init_type);
    sixel_set_kmeans_threshold_override(
        encoder->quantize_model_kmeans_threshold_override,
        encoder->quantize_model_kmeans_threshold);
    status = sixel_dither_initialize(*dither,
                                     palette_pixels,
                                     sixel_frame_get_width(palette_frame),
                                     sixel_frame_get_height(palette_frame),
                                     palette_pixelformat,
                                     encoder->method_for_largest,
                                     encoder->method_for_rep,
                                     encoder->quality_mode);
    sixel_set_kmeans_init_type_override(0, SIXEL_PALETTE_KMEANS_INIT_AUTO);
    sixel_set_kmeans_threshold_override(0, 0.125);
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

    histogram_colors = sixel_dither_get_num_of_histogram_colors(*dither);
    if (histogram_colors <= encoder->reqcolors) {
        encoder->method_for_diffuse = SIXEL_DIFFUSE_NONE;
    }
    sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));

    status = SIXEL_OK;

end:
    if (cluster_frame != NULL) {
        sixel_frame_unref(cluster_frame);
        cluster_frame = NULL;
    }
    if (SIXEL_SUCCEEDED(status) && dither != NULL && *dither != NULL) {
        sixel_dither_set_lut_policy(*dither, encoder->lut_policy);
        /* pass down the user's demand for an exact palette size */
        (*dither)->force_palette = encoder->force_palette;
    }
    return status;
}

static SIXELSTATUS
sixel_encoder_palette_builder(void *userdata,
                              sixel_frame_t *frame,
                              sixel_dither_t **dither_out,
                              sixel_logger_t *logger)
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

    status = SIXEL_FALSE;
    filter = NULL;
    palette = NULL;
    policy = SIXEL_LUT_POLICY_AUTO;
    memset(&result, 0, sizeof(result));
    memset(&lookup_config, 0, sizeof(lookup_config));
    memset(&fhedt_config, 0, sizeof(fhedt_config));
    memset(&vptree_config, 0, sizeof(vptree_config));
    memset(&eytzinger_config, 0, sizeof(eytzinger_config));

    if (encoder == NULL || dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    palette = dither->palette;
    if (palette == NULL || palette->entries == NULL
            || palette->depth <= 0 || palette->entry_count == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }

    policy = dither->lut_policy;
    if (policy != SIXEL_LUT_POLICY_FHEDT
            && policy != SIXEL_LUT_POLICY_VPTREE
            && policy != SIXEL_LUT_POLICY_EYTZINGER) {
        return SIXEL_OK;
    }

    lookup_config.palette = palette->entries;
    lookup_config.palette_float = palette->entries_float32;
    lookup_config.depth = palette->depth;
    lookup_config.float_depth = palette->float_depth;
    lookup_config.ncolors = (int)palette->entry_count;
    lookup_config.complexion = dither->complexion;
    lookup_config.method_for_largest = dither->method_for_largest;
    lookup_config.lut_policy = policy;
    lookup_config.pixelformat = dither->pixelformat;
    lookup_config.reuse_lut = palette->lut;

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
    if (SIXEL_SUCCEEDED(status) && result.lut != NULL) {
        if (palette->lut != NULL && palette->lut != result.lut) {
            sixel_lut_unref(palette->lut);
        }
        palette->lut = result.lut;
    }

    sixel_filter_free(filter);

    return status;
}


static void
sixel_debug_print_palette(
    sixel_dither_t /* in */ *dither /* dithering object */
)
{
    sixel_palette_t *palette_obj;
    unsigned char *palette_copy;
    size_t palette_count;
    int i;

    palette_obj = NULL;
    palette_copy = NULL;
    palette_count = 0U;
    if (dither == NULL) {
        return;
    }

    if (SIXEL_FAILED(
            sixel_dither_get_quantized_palette(dither, &palette_obj))
            || palette_obj == NULL) {
        return;
    }
    if (SIXEL_FAILED(sixel_palette_copy_entries_8bit(
            palette_obj,
            &palette_copy,
            &palette_count,
            SIXEL_PIXELFORMAT_RGB888,
            dither->allocator))
            || palette_copy == NULL) {
        sixel_palette_unref(palette_obj);
        return;
    }
    sixel_palette_unref(palette_obj);

    fprintf(stderr, "palette:\n");
    for (i = 0; i < (int)palette_count;
            ++i) {
        fprintf(stderr, "%d: #%02x%02x%02x\n", i,
                palette_copy[i * 3 + 0],
                palette_copy[i * 3 + 1],
                palette_copy[i * 3 + 2]);
    }
    sixel_allocator_free(dither->allocator, palette_copy);
}


static SIXELSTATUS
sixel_encoder_output_without_macro(
    sixel_frame_t       /* in */ *frame,
    sixel_dither_t      /* in */ *dither,
    sixel_output_t      /* in */ *output,
    sixel_encoder_t     /* in */ *encoder)
{
    SIXELSTATUS status = SIXEL_OK;
    static unsigned char *p;
    int depth;
    enum { message_buffer_size = 2048 };
    char message[message_buffer_size];
    int nwrite;
    int dulation;
    int delay;
    unsigned int remaining_delay;
    unsigned char *pixbuf;
    int width = 0;
    int height = 0;
    int pixelformat = 0;
    size_t size;
    int frame_colorspace = SIXEL_COLORSPACE_GAMMA;
    sixel_encoding_planner_t *planner;
#if defined(HAVE_CLOCK) || defined(HAVE_CLOCK_WIN)
    sixel_clock_t last_clock;
#endif

    if (encoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_without_macro: encoder object is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    planner = &encoder->planner;

    if (encoder->color_option == SIXEL_COLOR_OPTION_DEFAULT) {
        if (encoder->force_palette) {
            /* keep every slot when the user forced the palette size */
            sixel_dither_set_optimize_palette(dither, 0);
        } else {
            sixel_dither_set_optimize_palette(dither, 1);
        }
    }

    pixelformat = sixel_frame_get_pixelformat(frame);
    frame_colorspace = sixel_frame_get_colorspace(frame);
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    output->colorspace = encoder->output_colorspace;
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
    size = (size_t)(width * height * depth);

    sixel_encoder_log_stage(encoder,
                            frame,
                            "encode",
                            "worker",
                            "start",
                            "size=%dx%d fmt=%08x dst_cs=%d",
                            width,
                            height,
                            pixelformat,
                            output->colorspace);

    p = (unsigned char *)sixel_allocator_malloc(encoder->allocator, size);
    if (p == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_without_macro: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
#if defined(HAVE_CLOCK)
    if (output->last_clock == 0) {
        output->last_clock = clock();
    }
#elif defined(HAVE_CLOCK_WIN)
    if (output->last_clock == 0) {
        output->last_clock = clock_win();
    }
#endif
    delay = sixel_frame_get_delay(frame);
    if (delay > 0 && !encoder->fignore_delay && !encoder->fstatic) {
# if defined(HAVE_CLOCK)
        last_clock = clock();
/* https://stackoverflow.com/questions/16697005/clock-and-clocks-per-sec-on-osx-10-7 */
#  if defined(__APPLE__)
        dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                          / 100000);
#  else
        dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                          / CLOCKS_PER_SEC);
#  endif
        output->last_clock = last_clock;
# elif defined(HAVE_CLOCK_WIN)
        last_clock = clock_win();
        dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                          / CLOCKS_PER_SEC);
        output->last_clock = last_clock;
# else
        dulation = 0;
# endif
        if (dulation < 1000 * 10 * delay) {
            remaining_delay =
                (unsigned int)(1000 * 10 * delay - dulation);
            sixel_sleep(remaining_delay);
        }
    }

    pixbuf = sixel_frame_get_pixels(frame);
    memcpy(p, pixbuf, size);

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        goto end;
    }

    if (encoder->capture_quantized) {
        status = sixel_encoder_capture_quantized(encoder,
                                                 dither,
                                                 p,
                                                 size,
                                                 width,
                                                 height,
                                                 pixelformat,
                                                 frame_colorspace,
                                                 output->colorspace);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (planner != NULL && dither != NULL) {
        dither->pipeline_pin_threads = planner->pipeline_pin_threads;
    }
    status = sixel_encode(p, width, height, depth, dither, output);
    if (status != SIXEL_OK) {
        goto end;
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
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    sixel_allocator_free(encoder->allocator, p);

    return status;
}


static SIXELSTATUS
sixel_encoder_output_with_macro(
    sixel_frame_t   /* in */ *frame,
    sixel_dither_t  /* in */ *dither,
    sixel_output_t  /* in */ *output,
    sixel_encoder_t /* in */ *encoder)
{
    SIXELSTATUS status = SIXEL_OK;
    enum { message_buffer_size = 256 };
    char buffer[message_buffer_size];
    int nwrite;
    int dulation;
    int delay;
    unsigned int remaining_delay;
    int width;
    int height;
    int pixelformat;
    int depth;
    size_t size = 0;
    int frame_colorspace = SIXEL_COLORSPACE_GAMMA;
    unsigned char *converted = NULL;
    sixel_encoding_planner_t *planner;
#if defined(HAVE_CLOCK) || defined(HAVE_CLOCK_WIN)
    sixel_clock_t last_clock;
#endif

    planner = (encoder != NULL) ? &encoder->planner : NULL;

#if defined(HAVE_CLOCK)
    if (output->last_clock == 0) {
        output->last_clock = clock();
    }
#elif defined(HAVE_CLOCK_WIN)
    if (output->last_clock == 0) {
        output->last_clock = clock_win();
    }
#endif

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
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
    size = (size_t)width * (size_t)height * (size_t)depth;
    converted = (unsigned char *)sixel_allocator_malloc(
        encoder->allocator, size);
    if (converted == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_with_macro: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    memcpy(converted, sixel_frame_get_pixels(frame), size);
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    output->colorspace = encoder->output_colorspace;

    if (sixel_frame_get_loop_no(frame) == 0) {
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
                sixel_frame_get_frame_no(frame));
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
            sixel_frame_get_frame_no(frame));
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
    delay = sixel_frame_get_delay(frame);
    if (delay > 0 && !encoder->fignore_delay && !encoder->fstatic) {
# if defined(HAVE_CLOCK)
            last_clock = clock();
/* https://stackoverflow.com/questions/16697005/clock-and-clocks-per-sec-on-osx-10-7 */
#  if defined(__APPLE__)
            dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                             / 100000);
#  else
            dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                             / CLOCKS_PER_SEC);
#  endif
            output->last_clock = last_clock;
# elif defined(HAVE_CLOCK_WIN)
            last_clock = clock_win();
            dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                             / CLOCKS_PER_SEC);
            output->last_clock = last_clock;
# else
            dulation = 0;
# endif
            if (dulation < 1000 * 10 * delay) {
                remaining_delay =
                    (unsigned int)(1000 * 10 * delay - dulation);
                sixel_sleep(remaining_delay);
            }
        }
    }

end:
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    sixel_allocator_free(encoder->allocator, converted);

    return status;
}


static SIXELSTATUS
sixel_encoder_emit_iso2022_chars(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame
)
{
    char *buf_p, *buf;
    int col, row;
    int charset;
    int is_96cs;
    unsigned int charset_no;
    unsigned int code;
    int num_cols, num_rows;
    SIXELSTATUS status;
    size_t alloc_size;
    int nwrite;
    int target_fd;
    int chunk_size;

    charset_no = encoder->drcs_charset_no;
    if (charset_no == 0u) {
        charset_no = 1u;
    }
    if (encoder->drcs_mmv == 0) {
        is_96cs = (charset_no > 63u) ? 1 : 0;
        charset = (int)(((charset_no - 1u) % 63u) + 0x40u);
    } else if (encoder->drcs_mmv == 1) {
        is_96cs = 0;
        charset = (int)(charset_no + 0x3fu);
    } else {
        is_96cs = (charset_no > 79u) ? 1 : 0;
        charset = (int)(((charset_no - 1u) % 79u) + 0x30u);
    }
    code = 0x100020 + (is_96cs ? 0x80 : 0) + charset * 0x100;
    num_cols = (sixel_frame_get_width(frame) + encoder->cell_width - 1)
             / encoder->cell_width;
    num_rows = (sixel_frame_get_height(frame) + encoder->cell_height - 1)
             / encoder->cell_height;

    /* cols x rows + designation(4 chars) + SI + SO + LFs */
    alloc_size = num_cols * num_rows + (num_cols * num_rows / 96 + 1) * 4 + 2 + num_rows;
    buf_p = buf = sixel_allocator_malloc(encoder->allocator, alloc_size);
    if (buf == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_iso2022_chars: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    code = 0x20;
    *(buf_p++) = '\016';  /* SI */
    *(buf_p++) = '\033';
    *(buf_p++) = ')';
    *(buf_p++) = ' ';
    *(buf_p++) = (char)charset;
    for(row = 0; row < num_rows; row++) {
        for(col = 0; col < num_cols; col++) {
            if ((code & 0x7f) == 0x0) {
                if (charset == 0x7e) {
                    is_96cs = 1 - is_96cs;
                    charset = '0';
                } else {
                    charset++;
                }
                code = 0x20;
                *(buf_p++) = '\033';
                *(buf_p++) = is_96cs ? '-': ')';
                *(buf_p++) = ' ';
                *(buf_p++) = (char)charset;
            }
            *(buf_p++) = (char)code++;
        }
        *(buf_p++) = '\n';
    }
    *(buf_p++) = '\017';  /* SO */

    if (encoder->tile_outfd >= 0) {
        target_fd = encoder->tile_outfd;
    } else {
        target_fd = encoder->outfd;
    }

    chunk_size = (int)(buf_p - buf);
    nwrite = sixel_encoder_probe_fd_write(encoder,
                                          buf,
                                          chunk_size,
                                          target_fd);
    if (nwrite != chunk_size) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_iso2022_chars: write() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    sixel_allocator_free(encoder->allocator, buf);

    status = SIXEL_OK;

end:
    return status;
}


/*
 * This routine is derived from mlterm's drcssixel.c
 * (https://raw.githubusercontent.com/arakiken/mlterm/master/drcssixel/drcssixel.c).
 * The original implementation is credited to Araki Ken.
 * Adjusted here to integrate with libsixel's encoder pipeline.
 */
static SIXELSTATUS
sixel_encoder_emit_drcsmmv2_chars(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame
)
{
    char *buf_p, *buf;
    int col, row;
    char ibytes[3] = { 0x20, 0x00, 0x00 };
    int is_96cs;
    unsigned int charset_no;
    unsigned int code;
    int num_cols, num_rows;
    SIXELSTATUS status;
    size_t alloc_size;
    int nwrite;
    int target_fd;
    int chunk_size;
    int fill;

    charset_no = encoder->drcs_charset_no;
    if (charset_no == 0u) {
        charset_no = 1u;
    }
    if (encoder->drcs_mmv == 0) {
        is_96cs = (charset_no > 63u) ? 1 : 0;
        ibytes[1] = (char)(((charset_no - 1u) % 63u) + 0x40u);
        fill = 0;
    } else if (encoder->drcs_mmv == 1) {
        is_96cs = 0;
        ibytes[1] = (char)(charset_no + 0x3fu);
        fill = 0;
    } else if (encoder->drcs_mmv == 2) {
        is_96cs = (charset_no > 79u) ? 1 : 0;
        ibytes[1] = (char)(((charset_no - 1u) % 79u) + 0x30u);
        fill = 0;
    } else {  /* v3 */
        is_96cs = 0;
        ibytes[1] = (char)(((charset_no - 1u) / 63u) + 0x20u);
        ibytes[2] = (char)(((charset_no - 1u) % 63u) + 0x40u);
        fill = 1;
    }
    if (fill) {
        code = 0x100000 + (charset_no - 1u) * 94;
    } else {
        code = 0x100020 + (is_96cs ? 0x80 : 0) + ibytes[1] * 0x100;
    }
    num_cols = (sixel_frame_get_width(frame) + encoder->cell_width - 1)
             / encoder->cell_width;
    num_rows = (sixel_frame_get_height(frame) + encoder->cell_height - 1)
             / encoder->cell_height;

    /* cols x rows x 4(out of BMP) + rows(LFs) */
    alloc_size = num_cols * num_rows * 4 + num_rows;
    buf_p = buf = sixel_allocator_malloc(encoder->allocator, alloc_size);
    if (buf == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_drcsmmv2_chars: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (row = 0; row < num_rows; row++) {
        for (col = 0; col < num_cols; col++) {
            *(buf_p++) = ((code >> 18) & 0x07) | 0xf0;
            *(buf_p++) = ((code >> 12) & 0x3f) | 0x80;
            *(buf_p++) = ((code >> 6) & 0x3f) | 0x80;
            *(buf_p++) = (code & 0x3f) | 0x80;
            code++;
            if (! fill) {
                if ((code & 0x7f) == 0x0) {
                    if (ibytes[1] == 0x7e) {
                        is_96cs = 1 - is_96cs;
                        ibytes[1] = '0';
                    } else {
                        ibytes[1]++;
                    }
                    code = 0x100020 + (is_96cs ? 0x80 : 0) + ibytes[1] * 0x100;
                }
            }
        }
        *(buf_p++) = '\n';
    }

    if (encoder->tile_outfd >= 0) {
        target_fd = encoder->tile_outfd;
    } else {
        target_fd = encoder->outfd;
    }

    chunk_size = (int)(buf_p - buf);
    nwrite = sixel_encoder_probe_fd_write(encoder,
                                          buf,
                                          chunk_size,
                                          target_fd);
    if (nwrite != chunk_size) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_drcsmmv2_chars: write() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    sixel_allocator_free(encoder->allocator, buf);

    status = SIXEL_OK;

end:
    return status;
}

static SIXELSTATUS
sixel_encoder_encode_frame(
    sixel_encoder_t *encoder,
    sixel_frame_t   *frame,
    sixel_output_t  *output)
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

    memset(&context, 0, sizeof(context));
    context.encoder = encoder;
    context.frame = frame;
    context.output = output;
    context.status = SIXEL_FALSE;
    context.dither = NULL;
    context.height = 0;
    context.is_animation = 0;
    context.nwrite = 0;
    context.drcs_is_96cs_param = 0;
    context.drcs_designate_str[0] = 0x20;
    context.drcs_designate_str[1] = 0x20;
    context.drcs_designate_str[2] = 0x40;
    context.drcs_designate_str[3] = 0x00;
    context.fn_write = sixel_write_callback;
    context.write_callback = sixel_write_callback;
    context.scroll_callback = sixel_write_callback;
    context.write_priv = &encoder->outfd;
    context.scroll_priv = &encoder->outfd;
    context.probe.base_write = NULL;
    context.probe.base_priv = NULL;
    context.scroll_probe.base_write = NULL;
    context.scroll_probe.base_priv = NULL;
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
    memset(&context.dither_config, 0, sizeof(context.dither_config));
    context.current_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    context.current_colorspace = SIXEL_COLORSPACE_GAMMA;

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

    /*
     * Build the thread allocation plan up front so palette sampling does not
     * spawn extra workers when resize/clip/colorspace conversion already have
     * work to do on the main path.
     */
    if (planner != NULL) {
        sixel_encoding_planner_plan(planner, encoder, frame);
        target_pixelformat = planner->working_pixelformat;
    } else {
        target_pixelformat = sixel_encoder_pixelformat_for_colorspace(
            encoder->working_colorspace,
            encoder->prefer_float32);
    }

    current_pixelformat = sixel_frame_get_pixelformat(frame);
    current_colorspace = sixel_frame_get_colorspace(frame);

    palette_ready = sixel_encoding_palette_job_ready(encoder, planner, frame);
    if (planner != NULL) {
        sixel_encoding_planner_replan(planner,
                                      encoder,
                                      frame,
                                      palette_ready);
    }
    clip_active = (planner != NULL) ? planner->clip_active
        : (encoder->clipwidth > 0 && encoder->clipheight > 0);
    if (encoder->verbose) {
        sixel_encoding_planner_dump(planner,
                                    encoder,
                                    frame,
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
    (*ppencoder)->method_for_scan       = SIXEL_SCAN_AUTO;
    (*ppencoder)->method_for_carry      = SIXEL_CARRY_AUTO;
    (*ppencoder)->method_for_largest    = SIXEL_LARGE_AUTO;
    (*ppencoder)->method_for_rep        = SIXEL_REP_AUTO;
    (*ppencoder)->quality_mode          = SIXEL_QUALITY_AUTO;
    (*ppencoder)->quantize_model        = SIXEL_QUANTIZE_MODEL_AUTO;
    (*ppencoder)->quantize_model_kmeans_init_override = 0;
    (*ppencoder)->quantize_model_kmeans_init_type = SIXEL_PALETTE_KMEANS_INIT_AUTO;
    (*ppencoder)->quantize_model_kmeans_threshold_override = 0;
    (*ppencoder)->quantize_model_kmeans_threshold = 0.125;
    (*ppencoder)->final_merge_mode      = SIXEL_FINAL_MERGE_AUTO;
    (*ppencoder)->lut_policy            = SIXEL_LUT_POLICY_CERTLUT;
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
    (*ppencoder)->complexion            = 1;
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
    (*ppencoder)->outfd                 = STDOUT_FILENO;
    (*ppencoder)->tile_outfd            = (-1);
    (*ppencoder)->finsecure             = 0;
    (*ppencoder)->cancel_flag           = NULL;
    (*ppencoder)->dither_cache          = NULL;
    (*ppencoder)->drcs_charset_no       = 1u;
    (*ppencoder)->drcs_mmv              = 2;
    (*ppencoder)->capture_quantized     = 0;
    (*ppencoder)->capture_source        = 0;
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
    (*ppencoder)->capture_source_frame  = NULL;
    (*ppencoder)->last_loader_name[0]   = '\0';
    (*ppencoder)->last_source_path[0]   = '\0';
    (*ppencoder)->last_input_bytes      = 0u;
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
        if (encoder->capture_source_frame != NULL) {
            sixel_frame_unref(encoder->capture_source_frame);
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

/* set an option flag to encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_setopt(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ arg,
    char const      /* in */ *value)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int number;
    char lowered[16];
    size_t len;
    size_t i;
    long parsed_reqcolors;
    long parsed_value;
    char *endptr;
    int forced_palette;
    char *opt_copy;
    char const *suffix;
    int geometry_ok;
    char const *drcs_arg_delim;
    char const *drcs_arg_charset;
    char const *drcs_arg_second_delim;
    char const *drcs_arg_path;
    size_t drcs_arg_path_length;
    size_t drcs_segment_length;
    char drcs_segment[32];
    int drcs_mmv_value;
    long drcs_charset_value;
    unsigned int drcs_charset_limit;
    sixel_option_choice_result_t match_result;
    int match_value;
    sixel_option_argument_resolution_t q_resolution;
    size_t q_index;
    double q_threshold;
    char *q_endptr;
    sixel_suboption_assignment_t const *q_assignment;
    char const *q_key;
    char match_detail[128];
    char match_message[256];
    int png_argument_has_prefix = 0;
    char const *png_path_view = NULL;
    char *mapfile_copy;
    char *mapfile_copy_view;
    char *mapfile_normalized;
    size_t mapfile_offset;
    size_t mapfile_length;
    size_t png_path_length;
    size_t libc_buffer_size;
    size_t cell_prefix_length;
    size_t cell_detail_length;
    char cell_message[256];
    char const *cell_detail;
    char *libc_buffer;
    char const *libc_path;
    size_t mapfile_full_length;
    unsigned int path_flags;
    char const *mapfile_view;
    int path_check;

    sixel_encoder_ref(encoder);
    opt_copy = NULL;
    mapfile_copy = NULL;
    mapfile_copy_view = NULL;
    mapfile_offset = 0u;
    mapfile_length = 0u;
    path_flags = 0u;
    mapfile_view = NULL;
    path_check = 0;
    parsed_value = 0L;
    suffix = NULL;
    geometry_ok = 0;
    q_resolution.resolved_base_value = 0;
    q_resolution.base_def = NULL;
    q_resolution.assignments = NULL;
    q_resolution.assignment_count = 0u;
    q_index = 0u;
    q_threshold = 0.0;
    q_endptr = NULL;
    q_assignment = NULL;
    q_key = NULL;

    switch(arg) {
    case SIXEL_OPTFLAG_OUTFILE:  /* o */
        if (*value == '\0') {
            sixel_helper_set_additional_message(
                "no file name specified.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (is_png_target(value)) {
            encoder->output_is_png = 1;
            png_argument_has_prefix =
                (value != NULL)
                && (strncmp(value, "png:", 4) == 0);
            png_path_view = png_target_payload_view(value);
            if (png_argument_has_prefix
                    && (png_path_view == NULL
                        || png_path_view[0] == '\0')) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_setopt: missing target after the \"png:\" "
                    "prefix. use png:- or png:<path> with a non-empty payload."
                );
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            encoder->output_png_to_stdout =
                (png_path_view != NULL)
                && (strcmp(png_path_view, "-") == 0);
            sixel_allocator_free(encoder->allocator, encoder->png_output_path);
            encoder->png_output_path = NULL;
            sixel_allocator_free(encoder->allocator, encoder->sixel_output_path);
            encoder->sixel_output_path = NULL;
            if (! encoder->output_png_to_stdout) {
                /*
                 * +-----------------------------------------+
                 * |  PNG target normalization               |
                 * +-----------------------------------------+
                 * |  Raw input  |  Stored file path         |
                 * |-------------+---------------------------|
                 * |  png:-      |  "-" (stdout sentinel)    |
                 * |  png:/foo   |  "/foo"                   |
                 * +-----------------------------------------+
                 * Strip the "png:" prefix so the decoder can
                 * pass the true filesystem path to libpng
                 * while the CLI retains its shorthand.
                 */
                png_path_view = value;
                if (strncmp(value, "png:", 4) == 0) {
                    png_path_view = value + 4;
                }
                if (png_path_view[0] == '\0') {
                    sixel_helper_set_additional_message(
                        "sixel_encoder_setopt: PNG output path is empty.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                png_path_length = strlen(png_path_view);
                encoder->png_output_path =
                    (char *)sixel_allocator_malloc(
                        encoder->allocator, png_path_length + 1u);
                if (encoder->png_output_path == NULL) {
                    sixel_helper_set_additional_message(
                        "sixel_encoder_setopt: sixel_allocator_malloc() "
                        "failed for PNG output path.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                if (png_path_view != NULL) {
                    (void)sixel_compat_strcpy(encoder->png_output_path,
                                              png_path_length + 1u,
                                              png_path_view);
                } else {
                    encoder->png_output_path[0] = '\0';
                }
                libc_buffer_size = sixel_path_to_libc_buffer_size(
                    encoder->png_output_path);
                if (libc_buffer_size > 0u) {
                    libc_buffer = (char *)sixel_allocator_malloc(
                        encoder->allocator, libc_buffer_size);
                    if (libc_buffer == NULL) {
                        sixel_helper_set_additional_message(
                            "sixel_encoder_setopt: sixel_allocator_malloc() "
                            "failed for PNG path buffer.");
                        status = SIXEL_BAD_ALLOCATION;
                        goto end;
                    }
                    libc_path = sixel_path_to_libc(
                        encoder->png_output_path,
                        libc_buffer,
                        libc_buffer_size);
                    if (libc_path == NULL) {
                        sixel_helper_set_additional_message(
                            "sixel_encoder_setopt: invalid PNG output path.");
                        sixel_allocator_free(encoder->allocator, libc_buffer);
                        status = SIXEL_BAD_ARGUMENT;
                        goto end;
                    }
                    if (libc_path == libc_buffer) {
                        sixel_allocator_free(encoder->allocator,
                                             encoder->png_output_path);
                        encoder->png_output_path = libc_buffer;
                        libc_buffer = NULL;
                    }
                    if (libc_buffer != NULL) {
                        sixel_allocator_free(encoder->allocator, libc_buffer);
                    }
                }
            }
        } else {
            encoder->output_is_png = 0;
            encoder->output_png_to_stdout = 0;
            png_argument_has_prefix = 0;
            png_path_view = NULL;
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
            {
                sixel_clipboard_spec_t clipboard_spec;
                SIXELSTATUS clip_status;
                char *spool_path;
                int spool_fd;

                clipboard_spec.is_clipboard = 0;
                clipboard_spec.format[0] = '\0';
                clip_status = SIXEL_OK;
                spool_path = NULL;
                spool_fd = (-1);

                if (sixel_clipboard_parse_spec(value, &clipboard_spec)
                        && clipboard_spec.is_clipboard) {
                    clip_status = clipboard_create_spool(
                        encoder->allocator,
                        "clipboard-out",
                        &spool_path,
                        &spool_fd);
                    if (SIXEL_FAILED(clip_status)) {
                        status = clip_status;
                        goto end;
                    }
                    clipboard_select_format(
                        encoder->clipboard_output_format,
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
                    break;
                }

                if (spool_fd >= 0) {
                    (void)sixel_compat_close(spool_fd);
                }
                if (spool_path != NULL) {
                    sixel_allocator_free(encoder->allocator, spool_path);
                }
            }
            if (strcmp(value, "-") != 0) {
                encoder->sixel_output_path = (char *)sixel_allocator_malloc(
                    encoder->allocator, strlen(value) + 1);
                if (encoder->sixel_output_path == NULL) {
                    sixel_helper_set_additional_message(
                        "sixel_encoder_setopt: malloc() failed for output path.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                (void)sixel_compat_strcpy(encoder->sixel_output_path,
                                          strlen(value) + 1,
                                          value);
            }
        }

        if (!encoder->clipboard_output_active && strcmp(value, "-") != 0) {
            if (encoder->outfd && encoder->outfd != STDOUT_FILENO) {
                (void)sixel_compat_close(encoder->outfd);
            }
            encoder->outfd = sixel_compat_open(value,
                                               O_RDWR | O_CREAT | O_TRUNC,
                                               S_IRUSR | S_IWUSR);
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
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_precision,
            sizeof(g_option_choices_precision) /
                sizeof(g_option_choices_precision[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            status = sixel_encoder_apply_precision_override(
                encoder,
                (sixel_encoder_precision_mode_t)match_value);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(
                    value,
                    match_detail,
                    match_message,
                    sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "precision accepts auto, 8bit, or float32.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_THREADS:  /* = */
        if (sixel_encoder_parse_threads_argument(value, &number) == 0) {
            sixel_helper_set_additional_message(
                "threads accepts positive integers or 'auto'.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        sixel_set_threads(number);
        break;
    case SIXEL_OPTFLAG_COLORS:  /* p */
        forced_palette = 0;
        errno = 0;
        endptr = NULL;
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
                 * Negative palette sizes are rejected explicitly here rather
                 * than depending on strtol() overflow behavior. MSVCRT's
                 * strtol() can accept "-1" and return a valid number, so we
                 * enforce the positive range before attempting to parse.
                 */
                sixel_helper_set_additional_message(
                    "-p/--colors parameter must be 1 or more.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            parsed_reqcolors = strtol(value, &endptr, 10);
            if (endptr != NULL && *endptr == '!') {
                forced_palette = 1;
                ++endptr;
            }
            if (errno == ERANGE || endptr == value) {
                sixel_helper_set_additional_message(
                    "cannot parse -p/--colors option.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            if (endptr != NULL && *endptr != '\0') {
                sixel_helper_set_additional_message(
                    "cannot parse -p/--colors option.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        if (parsed_reqcolors < 1) {
            sixel_helper_set_additional_message(
                "-p/--colors parameter must be 1 or more.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (parsed_reqcolors > SIXEL_PALETTE_MAX) {
            sixel_helper_set_additional_message(
                "-p/--colors parameter must be less then or equal to 256.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->reqcolors = (int)parsed_reqcolors;
        encoder->force_palette = forced_palette;
        break;
    case SIXEL_OPTFLAG_MAPFILE:  /* m */
        mapfile_view = sixel_palette_strip_prefix(value, NULL);
        if (mapfile_view == NULL) {
            mapfile_view = value;
        }
        mapfile_length = strlen(value);
        mapfile_offset = (size_t)(mapfile_view - value);
        mapfile_copy = arg_strdup(value, encoder->allocator);
        if (mapfile_copy == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (mapfile_offset < mapfile_length) {
            mapfile_copy_view = mapfile_copy + mapfile_offset;
        } else {
            mapfile_copy_view = mapfile_copy;
        }
        /*
         * Normalize only the filesystem path portion so the stored value is
         * usable by the current libc while the original CLI token is kept for
         * diagnostics. The path prefix (TYPE:) remains untouched.
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
                mapfile_copy = NULL;
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            libc_path = sixel_path_to_libc(mapfile_copy_view,
                                           libc_buffer,
                                           libc_buffer_size);
            if (libc_path == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_setopt: invalid mapfile path.");
                sixel_allocator_free(encoder->allocator, libc_buffer);
                sixel_allocator_free(encoder->allocator, mapfile_copy);
                mapfile_copy = NULL;
                status = SIXEL_BAD_ARGUMENT;
                goto end;
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
                mapfile_copy = NULL;
                status = SIXEL_BAD_ALLOCATION;
                goto end;
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
        path_flags = SIXEL_OPTION_PATH_ALLOW_STDIN |
            SIXEL_OPTION_PATH_ALLOW_CLIPBOARD |
            SIXEL_OPTION_PATH_ALLOW_REMOTE |
            SIXEL_OPTION_PATH_ALLOW_EMPTY;
        path_check = sixel_option_validate_filesystem_path(
            value,
            mapfile_copy_view,
            path_flags);
        if (path_check != 0) {
            sixel_allocator_free(encoder->allocator, mapfile_copy);
            mapfile_copy = NULL;
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->mapfile) {
            sixel_allocator_free(encoder->allocator, encoder->mapfile);
        }
        encoder->mapfile = mapfile_copy;
        mapfile_copy = NULL;
        encoder->color_option = SIXEL_COLOR_OPTION_MAPFILE;
        break;
    case SIXEL_OPTFLAG_MAPFILE_OUTPUT:  /* M */
        if (value == NULL || *value == '\0') {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: mapfile-output path is empty.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        opt_copy = arg_strdup(value, encoder->allocator);
        if (opt_copy == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        status = sixel_encoder_enable_quantized_capture(encoder, 1);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(encoder->allocator, opt_copy);
            goto end;
        }
        sixel_allocator_free(encoder->allocator, encoder->palette_output);
        encoder->palette_output = opt_copy;
        opt_copy = NULL;
        break;
    case SIXEL_OPTFLAG_MONOCHROME:  /* e */
        encoder->color_option = SIXEL_COLOR_OPTION_MONOCHROME;
        break;
    case SIXEL_OPTFLAG_HIGH_COLOR:  /* I */
        encoder->color_option = SIXEL_COLOR_OPTION_HIGHCOLOR;
        break;
    case SIXEL_OPTFLAG_BUILTIN_PALETTE:  /* b */
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_builtin_palette,
            sizeof(g_option_choices_builtin_palette) /
            sizeof(g_option_choices_builtin_palette[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->builtin_palette = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "cannot parse builtin palette option.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->color_option = SIXEL_COLOR_OPTION_BUILTIN;
        break;
    case SIXEL_OPTFLAG_DIFFUSION:  /* d */
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_diffusion,
            sizeof(g_option_choices_diffusion) /
            sizeof(g_option_choices_diffusion[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->method_for_diffuse = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "specified diffusion method is not supported.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_DIFFUSION_SCAN:  /* y */
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_diffusion_scan,
            sizeof(g_option_choices_diffusion_scan) /
            sizeof(g_option_choices_diffusion_scan[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->method_for_scan = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "specified diffusion scan is not supported.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_DIFFUSION_CARRY:  /* Y */
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_diffusion_carry,
            sizeof(g_option_choices_diffusion_carry) /
            sizeof(g_option_choices_diffusion_carry[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->method_for_carry = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "specified diffusion carry mode is not supported.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_FIND_LARGEST:  /* f */
        if (value != NULL) {
            match_result = sixel_option_match_choice(
                value,
                g_option_choices_find_largest,
                sizeof(g_option_choices_find_largest) /
                sizeof(g_option_choices_find_largest[0]),
                &match_value,
                match_detail,
                sizeof(match_detail));
            if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
                encoder->method_for_largest = match_value;
            } else {
                if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                    sixel_option_report_ambiguous_prefix(value,
                                                  match_detail,
                                                  match_message,
                                                  sizeof(match_message));
                } else {
                    sixel_option_report_invalid_choice(
                        "specified finding method is not supported.",
                        match_detail,
                        match_message,
                        sizeof(match_message));
                }
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        break;
    case SIXEL_OPTFLAG_SELECT_COLOR:  /* s */
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_select_color,
            sizeof(g_option_choices_select_color) /
            sizeof(g_option_choices_select_color[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->method_for_rep = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "specified finding method is not supported.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_QUANTIZE_MODEL:  /* Q */
        /*
         * Parse MODEL[:KEY=VALUE]... in one pass so base value matching,
         * suboption key matching, and value suggestion diagnostics stay
         * consistent with the shared option matcher.
         */
        status = sixel_option_parse_argument_with_suboptions(
            value,
            &g_schema_quantize_model,
            &q_resolution,
            match_detail,
            sizeof(match_detail));
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        encoder->quantize_model = q_resolution.resolved_base_value;
        encoder->quantize_model_kmeans_init_override = 0;
        encoder->quantize_model_kmeans_threshold_override = 0;

        q_index = 0u;
        while (q_index < q_resolution.assignment_count) {
            q_assignment = q_resolution.assignments + q_index;
            q_key = q_assignment->resolved_key_name;
            if (q_key != NULL && strcmp(q_key, "inittype") == 0) {
                match_result = sixel_option_match_choice(
                    q_assignment->resolved_value_text,
                    (sixel_option_choice_t const *)
                    g_option_choices_kmeans_init_type,
                    sizeof(g_option_choices_kmeans_init_type)
                    / sizeof(g_option_choices_kmeans_init_type[0]),
                    &match_value,
                    match_detail,
                    sizeof(match_detail));
                if (match_result != SIXEL_OPTION_CHOICE_MATCH) {
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_init_override = 1;
                encoder->quantize_model_kmeans_init_type = match_value;
            } else if (q_key != NULL
                    && strcmp(q_key, "threshold") == 0) {
                errno = 0;
                q_endptr = NULL;
                q_threshold = strtod(q_assignment->resolved_value_text,
                                     &q_endptr);
                if (q_endptr == q_assignment->resolved_value_text
                        || q_endptr == NULL
                        || q_endptr[0] != '\0'
                        || errno != 0
                        || q_threshold < 0.0
                        || q_threshold > 0.5) {
                    sixel_helper_set_additional_message(
                        "-Q threshold must be in range 0.0-0.5.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                encoder->quantize_model_kmeans_threshold_override = 1;
                encoder->quantize_model_kmeans_threshold = q_threshold;
            }
            ++q_index;
        }
        break;
    case SIXEL_OPTFLAG_FINAL_MERGE:  /* F */
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_final_merge,
            sizeof(g_option_choices_final_merge) /
            sizeof(g_option_choices_final_merge[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->final_merge_mode = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "specified final merge policy is not supported.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_CROP:  /* c */
        geometry_ok = sixel_encoder_parse_crop_geometry(
            value,
            &encoder->clipwidth,
            &encoder->clipheight,
            &encoder->clipx,
            &encoder->clipy);
        if (!geometry_ok) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->clipfirst = 0;
        break;
    case SIXEL_OPTFLAG_WIDTH:  /* w */
        if (strcmp(value, "auto") == 0) {
            encoder->pixelwidth = (-1);
            encoder->percentwidth = (-1);
            break;
        }
        if (!sixel_encoder_parse_dimension_value(value,
                                                 &parsed_value,
                                                 &suffix)) {
            sixel_helper_set_additional_message(
                "cannot parse -w/--width option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (parsed_value > (long)INT_MAX) {
            sixel_helper_set_additional_message(
                "cannot parse -w/--width option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        number = (int)parsed_value;
        if (suffix[0] == '%' && suffix[1] == '\0') {
            if (number <= 0) {
                sixel_helper_set_additional_message(
                    "-w/--width percent must be 1 or more.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            encoder->pixelwidth = (-1);
            encoder->percentwidth = number;
        } else if (suffix[0] == 'c' && suffix[1] == '\0') {
            status = sixel_encoder_ensure_cell_size(encoder);
            if (SIXEL_FAILED(status)) {
                cell_detail = sixel_helper_get_additional_message();
                if (cell_detail != NULL && cell_detail[0] != '\0') {
                    /* Clamp the rendered detail to the fixed buffer size. */
                    cell_prefix_length = strlen("cannot determine terminal "
                                              "cell size for -w/--width "
                                              "option: ");
                    cell_detail_length = strnlen(cell_detail,
                                                 sizeof(cell_message));
                    if (cell_prefix_length + cell_detail_length
                            >= sizeof(cell_message)) {
                        cell_detail_length = sizeof(cell_message)
                                            - cell_prefix_length
                                            - 1U;
                    }
                    (void) snprintf(cell_message,
                                    sizeof(cell_message),
                                    "cannot determine terminal cell size for "
                                    "-w/--width option: %.*s",
                                    (int)cell_detail_length,
                                    cell_detail);
                    sixel_helper_set_additional_message(cell_message);
                } else {
                    sixel_helper_set_additional_message(
                        "cannot determine terminal cell size for "
                        "-w/--width option.");
                }
                goto end;
            }
            /*
             * Terminal cell units map the requested column count to pixels.
             * The cell size probe caches the tty geometry so repeated calls
             * reuse the same measurement.
             */
            if (number <= 0) {
                sixel_helper_set_additional_message(
                    "-w/--width cells must be 1 or more.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            encoder->pixelwidth = number * encoder->cell_width;
            encoder->percentwidth = (-1);
        } else if (suffix[0] == '\0' ||
                   (suffix[0] == 'p' && suffix[1] == 'x' &&
                    suffix[2] == '\0')) {
            if (number <= 0) {
                sixel_helper_set_additional_message(
                    "-w/--width must be 1 or more.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            encoder->pixelwidth = number;
            encoder->percentwidth = (-1);
        } else {
            sixel_helper_set_additional_message(
                "cannot parse -w/--width option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->clipwidth) {
            encoder->clipfirst = 1;
        }
        break;
    case SIXEL_OPTFLAG_HEIGHT:  /* h */
        if (strcmp(value, "auto") == 0) {
            encoder->pixelheight = (-1);
            encoder->percentheight = (-1);
            break;
        }
        if (!sixel_encoder_parse_dimension_value(value,
                                                 &parsed_value,
                                                 &suffix)) {
            sixel_helper_set_additional_message(
                "cannot parse -h/--height option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (parsed_value > (long)INT_MAX) {
            sixel_helper_set_additional_message(
                "cannot parse -h/--height option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        number = (int)parsed_value;
        if (suffix[0] == '%' && suffix[1] == '\0') {
            if (number <= 0) {
                sixel_helper_set_additional_message(
                    "-h/--height percent must be 1 or more.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            encoder->pixelheight = (-1);
            encoder->percentheight = number;
        } else if (suffix[0] == 'c' && suffix[1] == '\0') {
            status = sixel_encoder_ensure_cell_size(encoder);
            if (SIXEL_FAILED(status)) {
                cell_detail = sixel_helper_get_additional_message();
                if (cell_detail != NULL && cell_detail[0] != '\0') {
                    /* Clamp the rendered detail to the fixed buffer size. */
                    cell_prefix_length = strlen("cannot determine terminal "
                                              "cell size for -h/--height "
                                              "option: ");
                    cell_detail_length = strnlen(cell_detail,
                                                 sizeof(cell_message));
                    if (cell_prefix_length + cell_detail_length
                            >= sizeof(cell_message)) {
                        cell_detail_length = sizeof(cell_message)
                                            - cell_prefix_length
                                            - 1U;
                    }
                    (void) snprintf(cell_message,
                                    sizeof(cell_message),
                                    "cannot determine terminal cell size for "
                                    "-h/--height option: %.*s",
                                    (int)cell_detail_length,
                                    cell_detail);
                    sixel_helper_set_additional_message(cell_message);
                } else {
                    sixel_helper_set_additional_message(
                        "cannot determine terminal cell size for "
                        "-h/--height option.");
                }
                goto end;
            }
            /*
             * Rows specified in terminal cells use the current tty metrics to
             * translate into pixel counts before scaling.
             */
            if (number <= 0) {
                sixel_helper_set_additional_message(
                    "-h/--height cells must be 1 or more.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            encoder->pixelheight = number * encoder->cell_height;
            encoder->percentheight = (-1);
        } else if (suffix[0] == '\0' ||
                   (suffix[0] == 'p' && suffix[1] == 'x' &&
                    suffix[2] == '\0')) {
            if (number <= 0) {
                sixel_helper_set_additional_message(
                    "-h/--height must be 1 or more.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            encoder->pixelheight = number;
            encoder->percentheight = (-1);
        } else {
            sixel_helper_set_additional_message(
                "cannot parse -h/--height option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->clipheight) {
            encoder->clipfirst = 1;
        }
        break;
    case SIXEL_OPTFLAG_RESAMPLING:  /* r */
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_resampling,
            sizeof(g_option_choices_resampling) /
            sizeof(g_option_choices_resampling[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->method_for_resampling = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "specified desampling method is not supported.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_QUALITY:  /* q */
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_quality,
            sizeof(g_option_choices_quality) /
            sizeof(g_option_choices_quality[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->quality_mode = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "cannot parse quality option.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_LOOPMODE:  /* l */
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_loopmode,
            sizeof(g_option_choices_loopmode) /
            sizeof(g_option_choices_loopmode[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->loop_mode = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "cannot parse loop-control option.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_START_FRAME:  /* T */
        errno = 0;
        endptr = NULL;
        parsed_value = strtol(value, &endptr, 10);
        if (endptr == value || *endptr != '\0' || errno == ERANGE ||
            parsed_value < (long)INT_MIN ||
            parsed_value > (long)INT_MAX) {
            sixel_helper_set_additional_message(
                "cannot parse start_frame option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->loader_start_frame_no = (int)parsed_value;
        encoder->loader_start_frame_no_set = 1;
        break;
    case SIXEL_OPTFLAG_PALETTE_TYPE:  /* t */
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_palette_type,
            sizeof(g_option_choices_palette_type) /
            sizeof(g_option_choices_palette_type[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->palette_type = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "cannot parse palette type option.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
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
        errno = 0;
        endptr = NULL;
        parsed_value = strtol(value, &endptr, 10);
        if (endptr == value || *endptr != '\0' || errno == ERANGE ||
            parsed_value < 0L || parsed_value > (long)INT_MAX) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->macro_number = (int)parsed_value;
        break;
    case SIXEL_OPTFLAG_IGNORE_DELAY:  /* g */
        encoder->fignore_delay = 1;
        break;
    case SIXEL_OPTFLAG_VERBOSE:  /* v */
        encoder->verbose = 1;
        sixel_helper_set_loader_trace(1);
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
        encoder->fdrcs = 1;
        drcs_arg_delim = NULL;
        drcs_arg_charset = NULL;
        drcs_arg_second_delim = NULL;
        drcs_arg_path = NULL;
        drcs_arg_path_length = 0u;
        drcs_segment_length = 0u;
        drcs_mmv_value = 2;
        drcs_charset_value = 1L;
        drcs_charset_limit = 0u;
        if (value != NULL && *value != '\0') {
            drcs_arg_delim = strchr(value, ':');
            if (drcs_arg_delim == NULL) {
                drcs_segment_length = strlen(value);
                if (drcs_segment_length >= sizeof(drcs_segment)) {
                    sixel_helper_set_additional_message(
                        "DRCS mapping revision is too long.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                memcpy(drcs_segment, value, drcs_segment_length);
                drcs_segment[drcs_segment_length] = '\0';
                errno = 0;
                endptr = NULL;
                drcs_mmv_value = (int)strtol(drcs_segment, &endptr, 10);
                if (errno != 0 || endptr == drcs_segment || *endptr != '\0') {
                    sixel_helper_set_additional_message(
                        "cannot parse DRCS option.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
            } else {
                if (drcs_arg_delim != value) {
                    drcs_segment_length =
                        (size_t)(drcs_arg_delim - value);
                    if (drcs_segment_length >= sizeof(drcs_segment)) {
                        sixel_helper_set_additional_message(
                            "DRCS mapping revision is too long.");
                        status = SIXEL_BAD_ARGUMENT;
                        goto end;
                    }
                    memcpy(drcs_segment, value, drcs_segment_length);
                    drcs_segment[drcs_segment_length] = '\0';
                    errno = 0;
                    endptr = NULL;
                    drcs_mmv_value = (int)strtol(drcs_segment, &endptr, 10);
                    if (errno != 0 || endptr == drcs_segment || *endptr != '\0') {
                        sixel_helper_set_additional_message(
                            "cannot parse DRCS option.");
                        status = SIXEL_BAD_ARGUMENT;
                        goto end;
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
                            status = SIXEL_BAD_ARGUMENT;
                            goto end;
                        }
                        memcpy(drcs_segment,
                               drcs_arg_charset,
                               drcs_segment_length);
                        drcs_segment[drcs_segment_length] = '\0';
                        errno = 0;
                        endptr = NULL;
                        drcs_charset_value = strtol(drcs_segment,
                                                    &endptr,
                                                    10);
                        if (errno != 0 || endptr == drcs_segment ||
                                *endptr != '\0') {
                            sixel_helper_set_additional_message(
                                "cannot parse DRCS charset number.");
                            status = SIXEL_BAD_ARGUMENT;
                            goto end;
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
                        status = SIXEL_BAD_ARGUMENT;
                        goto end;
                    }
                    memcpy(drcs_segment,
                           drcs_arg_charset,
                           drcs_segment_length);
                    drcs_segment[drcs_segment_length] = '\0';
                    errno = 0;
                    endptr = NULL;
                    drcs_charset_value = strtol(drcs_segment,
                                                &endptr,
                                                10);
                    if (errno != 0 || endptr == drcs_segment ||
                            *endptr != '\0') {
                        sixel_helper_set_additional_message(
                            "cannot parse DRCS charset number.");
                        status = SIXEL_BAD_ARGUMENT;
                        goto end;
                    }
                }
            }
        }
        /*
         * Layout of the DRCS option value:
         *
         *    value = <mmv>:<charset_no>:<path>
         *          ^        ^                ^
         *          |        |                |
         *          |        |                +-- optional path that may reuse
         *          |        |                    STDOUT when set to "-" or drop
         *          |        |                    tiles when left blank
         *          |        +-- charset number (defaults to 1 when omitted)
         *          +-- mapping revision (defaults to 2 when omitted)
         */
        if (drcs_mmv_value < 0 || drcs_mmv_value > 3) {
            sixel_helper_set_additional_message(
                "unknown DRCS unicode mapping version.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
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
        if (drcs_charset_value < 1 ||
            (unsigned long)drcs_charset_value > drcs_charset_limit) {
            sixel_helper_set_additional_message(
                "DRCS charset number is out of range.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->drcs_mmv = drcs_mmv_value;
        encoder->drcs_charset_no = (unsigned short)drcs_charset_value;
        if (encoder->tile_outfd >= 0
            && encoder->tile_outfd != encoder->outfd
            && encoder->tile_outfd != STDOUT_FILENO
            && encoder->tile_outfd != STDERR_FILENO) {
            /*
             * Drop any previously opened tile stream through the
             * compatibility layer so that platform-specific details stay
             * centralized in src/compat_stub.c.
             */
            (void)sixel_compat_close(encoder->tile_outfd);
        }
        encoder->tile_outfd = (-1);
        if (drcs_arg_path != NULL) {
            if (strcmp(drcs_arg_path, "-") == 0) {
                encoder->tile_outfd = STDOUT_FILENO;
            } else {
                encoder->tile_outfd = sixel_compat_open(drcs_arg_path,
                                                       O_RDWR | O_CREAT |
                                                       O_TRUNC,
                                                       S_IRUSR | S_IWUSR);
                if (encoder->tile_outfd < 0) {
                    sixel_helper_set_additional_message(
                        "sixel_encoder_setopt: failed to open tile"
                        " output path.");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
            }
        }
        break;
    case SIXEL_OPTFLAG_PENETRATE:  /* P */
        encoder->penetrate_multiplexer = 1;
        break;
    case SIXEL_OPTFLAG_ENCODE_POLICY:  /* E */
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_encode_policy,
            sizeof(g_option_choices_encode_policy) /
            sizeof(g_option_choices_encode_policy[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->encode_policy = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "cannot parse encode policy option.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_LUT_POLICY:  /* ~ */
        match_result = sixel_option_match_choice(
            value,
            g_option_choices_lut_policy,
            sizeof(g_option_choices_lut_policy) /
            sizeof(g_option_choices_lut_policy[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->lut_policy = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_option_report_ambiguous_prefix(value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_option_report_invalid_choice(
                    "cannot parse lut policy option.",
                    match_detail,
                    match_message,
                    sizeof(match_message));
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->dither_cache != NULL) {
            sixel_dither_set_lut_policy(encoder->dither_cache,
                                        encoder->lut_policy);
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

            match_result = sixel_option_match_choice(
                lowered,
                g_option_choices_working_colorspace,
                sizeof(g_option_choices_working_colorspace) /
                sizeof(g_option_choices_working_colorspace[0]),
                &match_value,
                match_detail,
                sizeof(match_detail));
            if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
                encoder->clustering_colorspace = match_value;
                encoder->clustering_colorspace_set = 1;
                encoder->force_float32_colorspace = 1;
                encoder->prefer_float32 = 1;
            } else {
                if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                    sixel_option_report_ambiguous_prefix(value,
                        match_detail,
                        match_message,
                        sizeof(match_message));
                } else {
                    sixel_option_report_invalid_choice(
                        "unsupported clustering colorspace specified.",
                        match_detail,
                        match_message,
                        sizeof(match_message));
                }
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
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

            match_result = sixel_option_match_choice(
                lowered,
                g_option_choices_working_colorspace,
                sizeof(g_option_choices_working_colorspace) /
                sizeof(g_option_choices_working_colorspace[0]),
                &match_value,
                match_detail,
                sizeof(match_detail));
            if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
                encoder->working_colorspace = match_value;
                encoder->working_colorspace_set = 1;
                if (encoder->clustering_colorspace_set == 0) {
                    encoder->clustering_colorspace = match_value;
                }
                encoder->force_float32_colorspace = 1;
                encoder->prefer_float32 = 1;
            } else {
                if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                    sixel_option_report_ambiguous_prefix(value,
                        match_detail,
                        match_message,
                        sizeof(match_message));
                } else {
                    sixel_option_report_invalid_choice(
                        "unsupported working colorspace specified.",
                        match_detail,
                        match_message,
                        sizeof(match_message));
                }
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
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

            match_result = sixel_option_match_choice(
                lowered,
                g_option_choices_output_colorspace,
                sizeof(g_option_choices_output_colorspace) /
                sizeof(g_option_choices_output_colorspace[0]),
                &match_value,
                match_detail,
                sizeof(match_detail));
            if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
                encoder->output_colorspace = match_value;
            } else {
                if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                    sixel_option_report_ambiguous_prefix(value,
                        match_detail,
                        match_message,
                        sizeof(match_message));
                } else {
                    sixel_option_report_invalid_choice(
                        "unsupported output colorspace specified.",
                        match_detail,
                        match_message,
                        sizeof(match_message));
                }
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        break;
    case SIXEL_OPTFLAG_ORMODE:  /* O */
        encoder->ormode = 1;
        break;
    case SIXEL_OPTFLAG_COMPLEXION_SCORE:  /* C */
        errno = 0;
        endptr = NULL;
        parsed_value = strtol(value, &endptr, 10);
        if (endptr == value || *endptr != '\0' || errno == ERANGE ||
            parsed_value < 1L || parsed_value > (long)INT_MAX) {
            sixel_helper_set_additional_message(
                "complexion parameter must be 1 or more.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->complexion = (int)parsed_value;
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
    sixel_option_free_argument_resolution(&q_resolution);
    if (opt_copy != NULL) {
        sixel_allocator_free(encoder->allocator, opt_copy);
    }
    if (mapfile_copy != NULL) {
        sixel_allocator_free(encoder->allocator, mapfile_copy);
    }
    sixel_encoder_unref(encoder);

    return status;
}


static int
sixel_encoder_frame_pipeline_worker(void *priv)
{
    sixel_encoder_frame_pipeline_t *pipeline;
    sixel_frame_t *frame;
    SIXELSTATUS status;

    pipeline = (sixel_encoder_frame_pipeline_t *)priv;
    frame = NULL;
    status = SIXEL_OK;

    if (pipeline == NULL || pipeline->encoder == NULL) {
        return 0;
    }

    for (;;) {
        sixel_mutex_lock(&pipeline->mutex);
        while (pipeline->queue_count == 0
               && pipeline->loader_done == 0
               && SIXEL_SUCCEEDED(pipeline->worker_status)) {
            sixel_cond_wait(&pipeline->cond, &pipeline->mutex);
        }
        if (pipeline->queue_count == 0) {
            sixel_mutex_unlock(&pipeline->mutex);
            break;
        }
        frame = pipeline->queue[pipeline->queue_head];
        pipeline->queue[pipeline->queue_head] = NULL;
        pipeline->queue_head = (pipeline->queue_head + 1)
            % SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY;
        --pipeline->queue_count;
        sixel_cond_broadcast(&pipeline->cond);
        sixel_mutex_unlock(&pipeline->mutex);

        status = sixel_encoder_encode_frame(pipeline->encoder,
                                            frame,
                                            pipeline->output);
        sixel_frame_unref(frame);
        frame = NULL;
        if (SIXEL_FAILED(status)) {
            sixel_mutex_lock(&pipeline->mutex);
            pipeline->worker_status = status;
            pipeline->loader_done = 1;
            sixel_cond_broadcast(&pipeline->cond);
            sixel_mutex_unlock(&pipeline->mutex);
            break;
        }
    }

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
    sixel_frame_t *cloned_frame;

    status = SIXEL_OK;
    cloned_frame = NULL;

    if (pipeline == NULL || frame == NULL || pipeline->initialized == 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_encoder_clone_frame(frame,
                                       pipeline->encoder->allocator,
                                       &cloned_frame);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    sixel_mutex_lock(&pipeline->mutex);
    while (pipeline->queue_count >= SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY
           && SIXEL_SUCCEEDED(pipeline->worker_status)) {
        sixel_cond_wait(&pipeline->cond, &pipeline->mutex);
    }
    if (SIXEL_FAILED(pipeline->worker_status)) {
        status = pipeline->worker_status;
        sixel_mutex_unlock(&pipeline->mutex);
        sixel_frame_unref(cloned_frame);
        return status;
    }

    pipeline->queue[pipeline->queue_tail] = cloned_frame;
    pipeline->queue_tail = (pipeline->queue_tail + 1)
        % SIXEL_ENCODER_FRAME_PIPELINE_CAPACITY;
    ++pipeline->queue_count;
    sixel_cond_signal(&pipeline->cond);
    sixel_mutex_unlock(&pipeline->mutex);

    return status;
}


static SIXELSTATUS
sixel_encoder_frame_pipeline_finish(sixel_encoder_frame_pipeline_t *pipeline)
{
    SIXELSTATUS status;

    status = SIXEL_OK;

    if (pipeline == NULL || pipeline->initialized == 0) {
        return SIXEL_OK;
    }

    sixel_mutex_lock(&pipeline->mutex);
    pipeline->loader_done = 1;
    sixel_cond_broadcast(&pipeline->cond);
    sixel_mutex_unlock(&pipeline->mutex);

    if (pipeline->started != 0) {
        sixel_thread_join(&pipeline->thread);
        pipeline->started = 0;
    }

    sixel_mutex_lock(&pipeline->mutex);
    status = pipeline->worker_status;
    sixel_mutex_unlock(&pipeline->mutex);

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
    int result;
    int allow_loader_pipeline;

    context = (sixel_encoder_load_context_t *)data;
    pipeline = NULL;
    planner = NULL;
    encoder = NULL;
    status = SIXEL_OK;
    result = 0;
    allow_loader_pipeline = 0;

    if (context == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    encoder = context->encoder;
    planner = context->planner;
    pipeline = &context->frame_pipeline;
    if (encoder == NULL || pipeline == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (encoder->capture_source && encoder->capture_source_frame == NULL) {
        sixel_frame_ref(frame);
        encoder->capture_source_frame = frame;
    }

    if (pipeline->handoff_mode == SIXEL_ENCODER_HANDOFF_UNDECIDED
        && planner != NULL) {
        allow_loader_pipeline = sixel_encoding_planner_update_loader_handoff(
            planner,
            encoder,
            frame);
    }

    if (pipeline->handoff_mode == SIXEL_ENCODER_HANDOFF_UNDECIDED) {
        if (allow_loader_pipeline != 0) {
            result = sixel_thread_create(&pipeline->thread,
                                         sixel_encoder_frame_pipeline_worker,
                                         pipeline);
            if (result == 0) {
                pipeline->started = 1;
                pipeline->handoff_mode = SIXEL_ENCODER_HANDOFF_PIPELINE;
            } else {
                pipeline->handoff_mode = SIXEL_ENCODER_HANDOFF_SERIAL;
            }
        } else {
            pipeline->handoff_mode = SIXEL_ENCODER_HANDOFF_SERIAL;
        }
    }

    if (pipeline->handoff_mode == SIXEL_ENCODER_HANDOFF_PIPELINE) {
        status = sixel_encoder_frame_pipeline_enqueue(pipeline, frame);
        if (SIXEL_SUCCEEDED(status)) {
            return SIXEL_OK;
        }
        return status;
    }

    status = sixel_encoder_encode_frame(encoder, frame, context->output);

    return status;
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

    tmpdir = sixel_compat_getenv("TMPDIR");
#if defined(_WIN32)
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = sixel_compat_getenv("TEMP");
    }
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = sixel_compat_getenv("TMP");
    }
#endif
    if (tmpdir == NULL || tmpdir[0] == '\0') {
#if defined(_WIN32)
        tmpdir = ".";
#else
        tmpdir = "/tmp";
#endif
    }

    tmpdir_len = strlen(tmpdir);
    prefix_len = 0u;
    suffix_len = 0u;
    if (prefix == NULL) {
        return NULL;
    }

    prefix_len = strlen(prefix);
    suffix_len = prefix_len + strlen("-XXXXXX");
    maximum_tmpdir_len = (size_t)INT_MAX;

    if (maximum_tmpdir_len <= suffix_len + 2) {
        return NULL;
    }
    if (tmpdir_len > maximum_tmpdir_len - (suffix_len + 2)) {
        return NULL;
    }
    needs_separator = 1;
    if (tmpdir_len > 0) {
        if (tmpdir[tmpdir_len - 1] == '/' || tmpdir[tmpdir_len - 1] == '\\') {
            needs_separator = 0;
        }
    }

    template_len = tmpdir_len + suffix_len + 2;
    template_path = (char *)sixel_allocator_malloc(allocator, template_len);
    if (template_path == NULL) {
        return NULL;
    }

    if (needs_separator) {
#if defined(_WIN32)
        (void) snprintf(template_path, template_len,
                        "%s\\%s-XXXXXX", tmpdir, prefix);
#else
        (void) snprintf(template_path, template_len,
                        "%s/%s-XXXXXX", tmpdir, prefix);
#endif
    } else {
        (void) snprintf(template_path, template_len,
                        "%s%s-XXXXXX", tmpdir, prefix);
    }

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
    int fd;
    char *tmpname_result;

    status = SIXEL_FALSE;
    template_path = NULL;
    template_capacity = 0u;
    open_flags = 0;
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

    if (sixel_compat_mktemp(template_path, template_capacity) != 0) {
        tmpname_result = sixel_compat_tmpnam(template_path,
                                             template_capacity);
        if (tmpname_result == NULL) {
            sixel_helper_set_additional_message(
                "clipboard: failed to reserve spool template.");
            status = SIXEL_LIBC_ERROR;
            goto end;
        }
        template_capacity = strlen(template_path) + 1u;
    }

    open_flags = O_RDWR | O_CREAT | O_TRUNC;
#if defined(O_EXCL)
    open_flags |= O_EXCL;
#endif
    fd = sixel_compat_open(template_path, open_flags, S_IRUSR | S_IWUSR);
    if (fd < 0) {
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
    char *png_tmpnam_result = NULL;
    int png_open_flags = 0;
    int png_retry_flags = 0;
    sixel_clipboard_spec_t clipboard_spec;
    char clipboard_input_format[32];
    char *clipboard_input_path;
    unsigned char *clipboard_blob;
    size_t clipboard_blob_size;
    SIXELSTATUS clipboard_status;
    char const *effective_filename;
    unsigned int path_flags;
    int path_check;
    sixel_logger_t logger;
    int logger_prepared;
    sixel_encoder_load_context_t load_context;
    SIXELSTATUS pipeline_wait_status;

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
    memset(&load_context, 0, sizeof(load_context));
    sixel_logger_init(&logger);
    sixel_logger_prepare_env(&logger);
    logger_prepared = logger.active;
    if (encoder == NULL) {
        status = sixel_encoder_new(&encoder, NULL);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: sixel_encoder_new() failed.");
            goto end;
        }
    }
    sixel_encoder_ref(encoder);

    if (encoder != NULL) {
        encoder->logger = &logger;
        encoder->parallel_job_id = -1;
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

    if (encoder->output_is_png) {
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
        png_tmpnam_result = NULL;
        png_temp_path = create_temp_template(encoder->allocator,
                                             &png_temp_capacity);
        if (png_temp_path == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: malloc() failed for PNG staging path.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (sixel_compat_mktemp(png_temp_path, png_temp_capacity) != 0) {
            png_tmpnam_result = sixel_compat_tmpnam(png_temp_path,
                                                   png_temp_capacity);
            if (png_tmpnam_result == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_encode: mktemp() failed for PNG staging file.");
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            png_temp_capacity = strlen(png_temp_path) + 1u;
        }
        if (encoder->outfd >= 0 && encoder->outfd != STDOUT_FILENO) {
            (void)sixel_compat_close(encoder->outfd);
        }
        png_open_flags = O_RDWR | O_CREAT | O_TRUNC;
#if defined(O_EXCL)
        png_open_flags |= O_EXCL;
#endif
        png_retry_flags = png_open_flags;
#if defined(O_EXCL)
        png_retry_flags &= ~O_EXCL;
#endif
        encoder->outfd = sixel_compat_open(png_temp_path,
                                           png_open_flags,
                                           S_IRUSR | S_IWUSR);
#if defined(O_EXCL)
        if (encoder->outfd < 0
                && (errno == EBADF || errno == EINVAL)) {
            /*
             * Some virtual filesystems used by Emscripten reject O_EXCL
             * with EBADF/EINVAL even when the generated path is unique.
             * Retry without O_EXCL to avoid flaky prefixed PNG output.
             */
            encoder->outfd = sixel_compat_open(png_temp_path,
                                               png_retry_flags,
                                               S_IRUSR | S_IWUSR);
        }
#endif
        if (encoder->outfd < 0) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: failed to create the PNG target file.");
            status = SIXEL_LIBC_ERROR;
            goto end;
        }
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

    encoder->last_loader_name[0] = '\0';
    encoder->last_source_path[0] = '\0';
    encoder->last_input_bytes = 0u;

    /* if required color is not set, set the max value */
    if (encoder->reqcolors == (-1)) {
        encoder->reqcolors = SIXEL_PALETTE_MAX;
    }

    if (encoder->capture_source && encoder->capture_source_frame != NULL) {
        sixel_frame_unref(encoder->capture_source_frame);
        encoder->capture_source_frame = NULL;
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

    sixel_helper_set_loader_trace(encoder->verbose);
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
    encoder->last_input_bytes = sixel_loader_get_last_input_bytes(loader);
    if (sixel_loader_get_last_success_name(loader) != NULL) {
        (void)snprintf(encoder->last_loader_name,
                       sizeof(encoder->last_loader_name),
                       "%s",
                       sixel_loader_get_last_success_name(loader));
    } else {
        encoder->last_loader_name[0] = '\0';
    }
    if (sixel_loader_get_last_source_path(loader) != NULL) {
        (void)snprintf(encoder->last_source_path,
                       sizeof(encoder->last_source_path),
                       "%s",
                       sixel_loader_get_last_source_path(loader));
    } else {
        encoder->last_source_path[0] = '\0';
    }

load_end:
    sixel_loader_unref(loader);
    loader = NULL;

    pipeline_wait_status = sixel_encoder_frame_pipeline_finish(
        &load_context.frame_pipeline);
    if (SIXEL_SUCCEEDED(status) && SIXEL_FAILED(pipeline_wait_status)) {
        status = pipeline_wait_status;
    }

    if (status != SIXEL_OK) {
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
        while (encoder->cancel_flag && !*encoder->cancel_flag) {
            status = sixel_tty_wait_stdin(1000000);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            if (status != SIXEL_OK) {
                break;
            }
        }
        if (!encoder->cancel_flag || !*encoder->cancel_flag) {
            goto reload;
        }
    }

    if (encoder->output_is_png) {
        png_final_path = encoder->output_png_to_stdout ? "-" : encoder->png_output_path;
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
    (void)sixel_encoder_frame_pipeline_finish(&load_context.frame_pipeline);
    sixel_encoder_frame_pipeline_dispose(&load_context.frame_pipeline);
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
    sixel_allocator_free(encoder->allocator, png_temp_path);
    if (clipboard_input_path != NULL) {
        (void)sixel_compat_unlink(clipboard_input_path);
        sixel_allocator_free(encoder->allocator, clipboard_input_path);
    }
    if (clipboard_blob != NULL) {
        free(clipboard_blob);
    }
    if (encoder->clipboard_output_path != NULL) {
        (void)sixel_compat_unlink(encoder->clipboard_output_path);
        sixel_allocator_free(encoder->allocator,
                             encoder->clipboard_output_path);
        encoder->clipboard_output_path = NULL;
        encoder->sixel_output_path = NULL;
        encoder->clipboard_output_active = 0;
        encoder->clipboard_output_format[0] = '\0';
    }
    sixel_allocator_free(encoder->allocator, encoder->png_output_path);
    encoder->png_output_path = NULL;
    if (encoder != NULL) {
        encoder->logger = NULL;
        encoder->parallel_job_id = -1;
    }
    if (logger_prepared) {
        sixel_logger_close(&logger);
    }

    sixel_encoder_unref(encoder);

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
    size_t pixel_bytes;
    size_t pixel_total;
    size_t palette_bytes;
    int depth;

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
    if (pixel_total > SIZE_MAX / (size_t)depth) {
        sixel_helper_set_additional_message(
            "sixel_encoder_encode_bytes: buffer size overflow.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    pixel_bytes = pixel_total * (size_t)depth;
    owned_pixels = (unsigned char *)sixel_allocator_malloc(
        encoder->allocator, pixel_bytes);
    if (owned_pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_encode_bytes: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(owned_pixels, bytes, pixel_bytes);

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

    status = sixel_frame_new(&frame, encoder->allocator);
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
    if (pixel_total > SIZE_MAX / (size_t)depth) {
        sixel_helper_set_additional_message(
            "sixel_encoder_encode_bytes: buffer size overflow.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    pixel_bytes = pixel_total * (size_t)depth;
    owned_pixels = (unsigned char *)sixel_allocator_malloc(
        encoder->allocator, pixel_bytes);
    if (owned_pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_encode_bytes: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(owned_pixels, bytes, pixel_bytes);
    frame->pixels.u8ptr = owned_pixels;

    palette_bytes = 0u;
    if (palette != NULL && ncolors > 0) {
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
        frame->palette = owned_palette;
    }

    status = sixel_encoder_encode_frame(encoder, frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
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
 * Toggle source-frame capture for downstream consumers.
 */
SIXELAPI SIXELSTATUS
sixel_encoder_enable_source_capture(
    sixel_encoder_t *encoder,
    int enable)
{
    if (encoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_enable_source_capture: encoder is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    encoder->capture_source = enable ? 1 : 0;
    if (!encoder->capture_source && encoder->capture_source_frame != NULL) {
        sixel_frame_unref(encoder->capture_source_frame);
        encoder->capture_source_frame = NULL;
    }

    return SIXEL_OK;
}


/*
 * Enable or disable the quantized-frame capture facility.
 *
 *     capture on --> encoder keeps the latest palette-quantized frame.
 *     capture off --> encoder forgets previously stored frames.
 */
SIXELAPI SIXELSTATUS
sixel_encoder_enable_quantized_capture(
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
SIXELAPI SIXELSTATUS
sixel_encoder_copy_quantized_frame(
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

    status = sixel_frame_new(&frame, allocator);
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

    status = sixel_encoder_copy_quantized_frame(encoder,
                                                encoder->allocator,
                                                &frame);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    palette = (unsigned char const *)sixel_frame_get_palette(frame);
    exported_colors = sixel_frame_get_ncolors(frame);
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
        status = sixel_palette_write_act(stream, palette, exported_colors);
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


/*
 * Share the captured source frame with downstream consumers.
 */
SIXELAPI SIXELSTATUS
sixel_encoder_copy_source_frame(
    sixel_encoder_t *encoder,
    sixel_frame_t  **ppframe)
{
    if (encoder == NULL || ppframe == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_copy_source_frame: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (!encoder->capture_source || encoder->capture_source_frame == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_copy_source_frame: no frame captured.");
        return SIXEL_RUNTIME_ERROR;
    }

    sixel_frame_ref(encoder->capture_source_frame);
    *ppframe = encoder->capture_source_frame;

    return SIXEL_OK;
}




/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
