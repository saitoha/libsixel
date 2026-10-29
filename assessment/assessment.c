/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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

/*
 * assessment.c - High-speed image quality evaluator ported from Python.
 *
 *  +-------------------------------------------------------------+
 *  |                        PIPELINE MAP                         |
 *  +---------------------+------------------+--------------------+
 *  | image loading (RGB) | metric kernels   | JSON emit          |
 *  |  (libsixel loader)  |  (MS-SSIM, etc.) | (stdout + files)   |
 *  +---------------------+------------------+--------------------+
 *
 *  ASCII flow for the metrics modules:
 *
 *        +-----------+      +---------------+      +-------------+
 *        |   Luma    | ---> |  Spectral +   | ---> |  Composite  |
 *        |  Stack    |      |  Spatial      |      |   Report    |
 *        +-----------+      +---------------+      +-------------+
 *              |                    |                      |
 *              |                    |                      +--> JSON writer
 *              |                    +--> FFT / histogram engines
 *              +--> MS-SSIM / PSNR / GMSD
 *
 *  Every function carries intentionally verbose comments so that future
 *  maintainers can follow the numerical steps without cross-referencing the
 *  removed Python original.
 */

/*
 * Feature test macros are gated so unity builds do not emit redefinition
 * warnings when the compiler command line already requests the same POSIX
 * surface.  The guards keep the macros consistent without changing the
 * visibility of the libc extensions we rely on for mkstemp(), setenv(),
 * and strerror_r().
 */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "assessment.h"
#include "color.h"
#include "scielab.h"
#include "encoder.h"
#include "timer.h"
#include "sixel_atomic.h"

#if defined(_MSC_VER)
# define SIXEL_ASSESSMENT_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && \
    !defined(__STDC_NO_THREADS__)
# define SIXEL_ASSESSMENT_TLS _Thread_local
#else
# define SIXEL_ASSESSMENT_TLS
#endif

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#if defined(__linux__)
#include <sys/stat.h>
#include <sys/types.h>
#endif


#if defined(_WIN32)
#define SIXEL_PATH_SEP '\\'
#define SIXEL_PATH_LIST_SEP ';'
#else
#define SIXEL_PATH_SEP '/'
#define SIXEL_PATH_LIST_SEP ':'
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum { SIXEL_ASSESSMENT_RGB_CHANNELS = 3 };

typedef struct sixel_assessment_float_buffer {
    size_t length;
    float *values;
} sixel_assessment_float_buffer_t;

typedef struct sixel_assessment_complex {
    double re;
    double im;
} sixel_assessment_complex_t;

typedef struct sixel_assessment_metrics {
    unsigned int valid_mask; /* bit mask for computed metrics */
    float ms_ssim;
    float vif_p;
    float high_freq_out;
    float high_freq_ref;
    float high_freq_delta;
    float stripe_ref;
    float stripe_out;
    float stripe_rel;
    float band_run_rel;
    float band_grad_rel;
    float clip_l_ref;
    float clip_r_ref;
    float clip_g_ref;
    float clip_b_ref;
    float clip_l_out;
    float clip_r_out;
    float clip_g_out;
    float clip_b_out;
    float clip_l_rel;
    float clip_r_rel;
    float clip_g_rel;
    float clip_b_rel;
    float delta_chroma_mean;
    float delta_e00_mean;
    float delta_e00_max;
    float gmsd_value;
    float psnr_y;
} sixel_assessment_metrics_t;

typedef struct sixel_assessment_capture {
    sixel_frame_t *frame;
} sixel_assessment_capture_t;

static sixel_assessment_t *g_assessment_context = NULL;
static sixel_assessment_t *g_active_encode_assessment = NULL;

static unsigned int
assessment_metric_mask_all(void)
{
#if SIXEL_ASSESSMENT_METRIC_COUNT >= 32
    return 0xffffffffu;
#else
    return (1u << SIXEL_ASSESSMENT_METRIC_COUNT) - 1u;
#endif
}

static int
assessment_metric_selected(sixel_assessment_t const *assessment,
                           int metric_id)
{
    unsigned int mask;
    unsigned int index;

    if (assessment == NULL) {
        return 0;
    }
    index = (unsigned int)SIXEL_ASSESSMENT_INDEX(metric_id);
    if (index >= (unsigned int)SIXEL_ASSESSMENT_METRIC_COUNT) {
        return 0;
    }
    mask = assessment->metrics_mask;
    return (mask & (1u << index)) != 0u;
}

typedef struct assessment_stage_descriptor {
    sixel_assessment_stage_t id;
    char const *label;
} assessment_stage_descriptor_t;

static assessment_stage_descriptor_t const g_stage_descriptors[] = {
    {SIXEL_ASSESSMENT_STAGE_IMAGE_CHUNK, "ImageRead"},
    {SIXEL_ASSESSMENT_STAGE_IMAGE_DECODE, "ImageDecode"},
    {SIXEL_ASSESSMENT_STAGE_SCALE, "Scale"},
    {SIXEL_ASSESSMENT_STAGE_CROP, "Crop"},
    {SIXEL_ASSESSMENT_STAGE_COLORSPACE, "ColorConvert"},
    {SIXEL_ASSESSMENT_STAGE_PALETTE_HISTOGRAM, "PaletteHistogram"},
    {SIXEL_ASSESSMENT_STAGE_PALETTE_SOLVE, "PaletteSolve"},
    {SIXEL_ASSESSMENT_STAGE_PALETTE_APPLY, "PaletteApply"},
    {SIXEL_ASSESSMENT_STAGE_ENCODE, "Encode"},
    {SIXEL_ASSESSMENT_STAGE_ENCODE_PREPARE, "EncodePrepare"},
    {SIXEL_ASSESSMENT_STAGE_ENCODE_CLASSIFY, "EncodeClassify"},
    {SIXEL_ASSESSMENT_STAGE_ENCODE_COMPOSE, "EncodeCompose"},
    {SIXEL_ASSESSMENT_STAGE_ENCODE_COMPOSE_SCAN, "EncodeComposeScan"},
    {SIXEL_ASSESSMENT_STAGE_ENCODE_COMPOSE_QUEUE, "EncodeComposeQueue"},
    {SIXEL_ASSESSMENT_STAGE_ENCODE_EMIT, "EncodeEmit"},
    {SIXEL_ASSESSMENT_STAGE_OUTPUT, "Output"}
};

/*
 * Only top-level stages contribute to the total so nested probes do not
 * inflate the reported wall time by double counting their parents.
 */
static unsigned char const g_stage_counts_toward_total[] = {
    1, /* ImageRead */
    1, /* ImageDecode */
    1, /* Scale */
    1, /* Crop */
    1, /* ColorConvert */
    1, /* PaletteHistogram */
    1, /* PaletteSolve */
    1, /* PaletteApply */
    1, /* Encode */
    0, /* EncodePrepare */
    0, /* EncodeClassify */
    0, /* EncodeCompose */
    0, /* EncodeComposeScan */
    0, /* EncodeComposeQueue */
    0, /* EncodeEmit */
    1  /* Output */
};

static int g_encode_parallel_threads = 1;
static int g_encode_substages_visible = 1;

static int
sixel_assessment_stage_is_encode_detail(sixel_assessment_stage_t stage)
{
    switch (stage) {
    case SIXEL_ASSESSMENT_STAGE_ENCODE_PREPARE:
    case SIXEL_ASSESSMENT_STAGE_ENCODE_CLASSIFY:
    case SIXEL_ASSESSMENT_STAGE_ENCODE_COMPOSE:
    case SIXEL_ASSESSMENT_STAGE_ENCODE_COMPOSE_SCAN:
    case SIXEL_ASSESSMENT_STAGE_ENCODE_COMPOSE_QUEUE:
    case SIXEL_ASSESSMENT_STAGE_ENCODE_EMIT:
        return 1;
    default:
        break;
    }
    return 0;
}

static int
sixel_assessment_stage_should_emit(size_t stage_index)
{
    sixel_assessment_stage_t stage;

    stage = g_stage_descriptors[stage_index].id;
    if (!g_encode_substages_visible &&
            sixel_assessment_stage_is_encode_detail(stage)) {
        return 0;
    }
    return 1;
}

static void
assessment_reset_stage_bookkeeping(sixel_assessment_t *assessment)
{
    if (assessment == NULL) {
        return;
    }
    assessment->active_stage = SIXEL_ASSESSMENT_STAGE_NONE;
    assessment->stage_started_at = 0.0;
    assessment->stage_active = 0;
    memset(assessment->stage_durations, 0,
           sizeof(assessment->stage_durations));
    memset(assessment->stage_bytes, 0, sizeof(assessment->stage_bytes));
    assessment->output_bytes_written = 0u;
    assessment->encode_output_time_pending = 0.0;
}

/*
 * Local bounded string copy helper for LSQA.
 *
 * assessment/ must not depend on src/compat_stub.h because that header is
 * intended for private core sources under src/.  Keep a tiny local helper so
 * this translation unit can safely copy short labels without crossing layers.
 */
static int
lsqa_compat_strcpy(char *destination,
                   size_t destination_size,
                   char const *source)
{
    size_t length;

    if (destination == NULL || source == NULL || destination_size == 0) {
        return (-1);
    }

    length = strlen(source);
    if (length >= destination_size) {
        length = destination_size - 1;
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
    return (int)length;
}

static void
assessment_guess_format(sixel_assessment_t *assessment)
{
    char const *loader;
    char const *extension;
    size_t len;
    size_t index;

    if (assessment == NULL) {
        return;
    }
    if (assessment->format_name[0] != '\0') {
        return;
    }
    loader = assessment->loader_name;
    if (loader[0] != '\0') {
        if (strcmp(loader, "libpng") == 0) {
            (void)lsqa_compat_strcpy(assessment->format_name,
                                     sizeof(assessment->format_name),
                                     "png");
            return;
        } else if (strcmp(loader, "libjpeg") == 0) {
            (void)lsqa_compat_strcpy(assessment->format_name,
                                     sizeof(assessment->format_name),
                                     "jpeg");
            return;
        } else if (strcmp(loader, "wic") == 0) {
            (void)lsqa_compat_strcpy(assessment->format_name,
                                     sizeof(assessment->format_name),
                                     "wic");
            return;
        } else if (strcmp(loader, "builtin") == 0) {
            (void)lsqa_compat_strcpy(assessment->format_name,
                                     sizeof(assessment->format_name),
                                     "builtin");
            return;
        }
    }
    extension = strrchr(assessment->input_path, '.');
    if (extension != NULL && extension[1] != '\0') {
        len = strlen(extension + 1);
        if (len >= sizeof(assessment->format_name)) {
            len = sizeof(assessment->format_name) - 1;
        }
        for (index = 0; index < len; ++index) {
            assessment->format_name[index] =
                (char)tolower((unsigned char)extension[1 + index]);
        }
        assessment->format_name[len] = '\0';
    } else {
        (void)lsqa_compat_strcpy(assessment->format_name,
                                 sizeof(assessment->format_name),
                                 "unknown");
    }
}

static int
assessment_escape_json(char const *input,
                       char *output,
                       size_t output_size)
{
    size_t index;
    size_t written;
    unsigned char ch;
    int n;

    if (output == NULL || output_size == 0) {
        return -1;
    }
    written = 0;
    if (input == NULL) {
        output[0] = '\0';
        return 0;
    }
    for (index = 0; input[index] != '\0'; ++index) {
        ch = (unsigned char)input[index];
        if (ch == '"' || ch == '\\') {
            if (written + 2 >= output_size) {
                return -1;
            }
            output[written++] = '\\';
            output[written++] = (char)ch;
        } else if (ch <= 0x1F) {
            if (written + 6 >= output_size) {
                return -1;
            }
            /*
             * pcc misparses "\\u...." literals in this context.  Emit the
             * backslash + 'u' prefix explicitly, then append four hex digits.
             */
            output[written++] = '\\';
            output[written++] = 'u';
            n = snprintf(output + written, output_size - written,
                         "%04x", ch);
            if (n < 0 || (size_t)n >= output_size - written) {
                return -1;
            }
            written += (size_t)n;
        } else {
            if (written + 1 >= output_size) {
                return -1;
            }
            output[written++] = (char)ch;
        }
    }
    if (written >= output_size) {
        return -1;
    }
    output[written] = '\0';
    return 0;
}

static char const *
assessment_pixelformat_name(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_PAL1:
        return "PAL1";
    case SIXEL_PIXELFORMAT_PAL2:
        return "PAL2";
    case SIXEL_PIXELFORMAT_PAL4:
        return "PAL4";
    case SIXEL_PIXELFORMAT_PAL8:
        return "PAL8";
    case SIXEL_PIXELFORMAT_RGB555:
        return "RGB555";
    case SIXEL_PIXELFORMAT_RGB565:
        return "RGB565";
    case SIXEL_PIXELFORMAT_RGB888:
        return "RGB888";
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
        return "RGBFLOAT32";
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
        return "LINEARRGBFLOAT32";
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
        return "OKLABFLOAT32";
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
        return "CIELABFLOAT32";
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
        return "DIN99DFLOAT32";
    case SIXEL_PIXELFORMAT_BGR555:
        return "BGR555";
    case SIXEL_PIXELFORMAT_BGR565:
        return "BGR565";
    case SIXEL_PIXELFORMAT_BGR888:
        return "BGR888";
    case SIXEL_PIXELFORMAT_G1:
        return "G1";
    case SIXEL_PIXELFORMAT_G2:
        return "G2";
    case SIXEL_PIXELFORMAT_G4:
        return "G4";
    case SIXEL_PIXELFORMAT_G8:
        return "G8";
    default:
        break;
    }
    return "unknown";
}

SIXEL_INTERNAL_API char const *
sixel_assessment_colorspace_name(int colorspace)
{
    switch (colorspace) {
    case SIXEL_COLORSPACE_GAMMA:
        return "gamma";
    case SIXEL_COLORSPACE_LINEAR:
        return "linear";
    case SIXEL_COLORSPACE_OKLAB:
        return "oklab";
    case SIXEL_COLORSPACE_CIELAB:
        return "cielab";
    case SIXEL_COLORSPACE_DIN99D:
        return "din99d";
    case SIXEL_COLORSPACE_SMPTEC:
        return "smpte-c";
    default:
        break;
    }
    return "unknown";
}

SIXELAPI void
sixel_assessment_stage_transition(sixel_assessment_t *assessment,
                                  sixel_assessment_stage_t stage)
{
    double now;
    double elapsed;
    sixel_assessment_stage_t previous_stage;
    double total_pending;

    if (assessment == NULL) {
        return;
    }
    if ((assessment->sections_mask &
            SIXEL_ASSESSMENT_SECTION_PERFORMANCE) == 0u) {
        return;
    }
    if (stage <= SIXEL_ASSESSMENT_STAGE_NONE ||
            stage >= SIXEL_ASSESSMENT_STAGE_COUNT) {
        return;
    }
    now = sixel_timer_now();
    previous_stage = assessment->active_stage;
    if (assessment->stage_active &&
            previous_stage > SIXEL_ASSESSMENT_STAGE_NONE &&
            previous_stage < SIXEL_ASSESSMENT_STAGE_COUNT) {
        elapsed = now - assessment->stage_started_at;
        if (previous_stage != SIXEL_ASSESSMENT_STAGE_OUTPUT) {
            /* Output spans rely on explicit fn_write() timing. */
            assessment->stage_durations[previous_stage] += elapsed;
        }
        if (previous_stage == SIXEL_ASSESSMENT_STAGE_ENCODE &&
                (assessment->encode_output_time_pending > 0.0 ||
                 assessment->encode_palette_time_pending > 0.0)) {
            /*
             * Rebalance the encode slot so that we only keep the pure
             * encoder core time.  The write spans move to the output
             * bucket, and the palette spans return to the palette
             * bucket.  The ASCII map shows how the same wall-clock
             * region is split across the stages.
             *
             *     encode wall time
             *     +----------+-----------+------------------+
             *     | palette  | encode    | write callback ->|
             *     | (return) | core stay | (output bucket) |
             *     +----------+-----------+------------------+
             */
            total_pending = assessment->encode_output_time_pending +
                assessment->encode_palette_time_pending;
            assessment->stage_durations[previous_stage] -= total_pending;
            if (assessment->stage_durations[previous_stage] < 0.0) {
                assessment->stage_durations[previous_stage] = 0.0;
            }
            assessment->encode_output_time_pending = 0.0;
            assessment->encode_palette_time_pending = 0.0;
            if (g_active_encode_assessment == assessment) {
                g_active_encode_assessment = NULL;
            }
        }
    }
    assessment->active_stage = stage;
    assessment->stage_started_at = now;
    assessment->stage_active = 1;
    if (stage == SIXEL_ASSESSMENT_STAGE_ENCODE) {
        assessment->encode_output_time_pending = 0.0;
        assessment->encode_palette_time_pending = 0.0;
        g_active_encode_assessment = assessment;
    } else if (g_active_encode_assessment == assessment) {
        g_active_encode_assessment = NULL;
    }
}

SIXELAPI void
sixel_assessment_stage_finish(sixel_assessment_t *assessment)
{
    double now;
    double elapsed;
    sixel_assessment_stage_t finished_stage;
    double total_pending;

    if (assessment == NULL) {
        return;
    }
    if ((assessment->sections_mask &
            SIXEL_ASSESSMENT_SECTION_PERFORMANCE) == 0u) {
        assessment->stage_active = 0;
        assessment->active_stage = SIXEL_ASSESSMENT_STAGE_NONE;
        assessment->stage_started_at = 0.0;
        assessment->encode_output_time_pending = 0.0;
        if (g_active_encode_assessment == assessment) {
            g_active_encode_assessment = NULL;
        }
        return;
    }
    if (!assessment->stage_active ||
            assessment->active_stage <= SIXEL_ASSESSMENT_STAGE_NONE ||
            assessment->active_stage >= SIXEL_ASSESSMENT_STAGE_COUNT) {
        assessment->stage_active = 0;
        assessment->active_stage = SIXEL_ASSESSMENT_STAGE_NONE;
        assessment->stage_started_at = 0.0;
        assessment->encode_output_time_pending = 0.0;
        return;
    }
    now = sixel_timer_now();
    elapsed = now - assessment->stage_started_at;
    finished_stage = assessment->active_stage;
    if (finished_stage != SIXEL_ASSESSMENT_STAGE_OUTPUT) {
        /* Output spans rely on explicit fn_write() timing. */
        assessment->stage_durations[finished_stage] += elapsed;
    }
    if (finished_stage == SIXEL_ASSESSMENT_STAGE_ENCODE &&
            (assessment->encode_output_time_pending > 0.0 ||
             assessment->encode_palette_time_pending > 0.0)) {
        /* Mirror the transition logic for the final leg. */
        total_pending = assessment->encode_output_time_pending +
            assessment->encode_palette_time_pending;
        assessment->stage_durations[finished_stage] -= total_pending;
        if (assessment->stage_durations[finished_stage] < 0.0) {
            assessment->stage_durations[finished_stage] = 0.0;
        }
        assessment->encode_output_time_pending = 0.0;
        assessment->encode_palette_time_pending = 0.0;
        if (g_active_encode_assessment == assessment) {
            g_active_encode_assessment = NULL;
        }
    }
    assessment->stage_active = 0;
    assessment->active_stage = SIXEL_ASSESSMENT_STAGE_NONE;
    assessment->stage_started_at = 0.0;
}

SIXELAPI void
sixel_assessment_record_stage_duration(sixel_assessment_t *assessment,
                                       sixel_assessment_stage_t stage,
                                       double duration)
{
    if (assessment == NULL) {
        return;
    }
    if ((assessment->sections_mask &
            SIXEL_ASSESSMENT_SECTION_PERFORMANCE) == 0u) {
        return;
    }
    if (stage <= SIXEL_ASSESSMENT_STAGE_NONE ||
            stage >= SIXEL_ASSESSMENT_STAGE_COUNT) {
        return;
    }
    if (duration < 0.0) {
        duration = 0.0;
    }
    assessment->stage_durations[stage] += duration;
}

SIXELAPI void
sixel_assessment_record_loader(sixel_assessment_t *assessment,
                               char const *path,
                               char const *loader_name,
                               size_t input_bytes)
{
    unsigned int mask;

    if (assessment == NULL) {
        return;
    }
    mask = assessment->sections_mask;
    if ((mask & (SIXEL_ASSESSMENT_SECTION_BASIC |
                 SIXEL_ASSESSMENT_SECTION_PERFORMANCE)) == 0u) {
        return;
    }
    if ((mask & SIXEL_ASSESSMENT_SECTION_BASIC) != 0u) {
        if (path != NULL) {
            (void)snprintf(assessment->input_path,
                           sizeof(assessment->input_path),
                           "%s",
                           path);
        } else {
            assessment->input_path[0] = '\0';
        }
        if (loader_name != NULL) {
            (void)snprintf(assessment->loader_name,
                           sizeof(assessment->loader_name),
                           "%s",
                           loader_name);
        } else {
            assessment->loader_name[0] = '\0';
        }
        assessment->input_bytes = input_bytes;
        assessment_guess_format(assessment);
    }
    if ((mask & SIXEL_ASSESSMENT_SECTION_PERFORMANCE) != 0u) {
        assessment->stage_bytes[SIXEL_ASSESSMENT_STAGE_IMAGE_CHUNK] +=
            input_bytes;
    }
}

SIXELAPI void
sixel_assessment_record_source_frame(sixel_assessment_t *assessment,
                                     sixel_frame_t *frame)
{
    int width;
    int height;
    int pixelformat;
    int colorspace;
    int depth;
    size_t bytes;
    unsigned int mask;

    if (assessment == NULL || frame == NULL) {
        return;
    }
    mask = assessment->sections_mask;
    if ((mask & (SIXEL_ASSESSMENT_SECTION_BASIC |
                 SIXEL_ASSESSMENT_SECTION_SIZE |
                 SIXEL_ASSESSMENT_SECTION_PERFORMANCE)) == 0u) {
        return;
    }
    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    pixelformat = sixel_frame_get_pixelformat(frame);
    colorspace = sixel_frame_get_colorspace(frame);
    depth = sixel_helper_compute_depth(pixelformat);
    if (depth <= 0 || width <= 0 || height <= 0) {
        bytes = 0u;
    } else {
        bytes = (size_t)width;
        bytes *= (size_t)height;
        bytes *= (size_t)depth;
    }
    if ((mask & (SIXEL_ASSESSMENT_SECTION_BASIC |
                 SIXEL_ASSESSMENT_SECTION_SIZE)) != 0u) {
        assessment->input_pixelformat = pixelformat;
        assessment->input_colorspace = colorspace;
        assessment->source_pixels_bytes = bytes;
    }
    if ((mask & SIXEL_ASSESSMENT_SECTION_PERFORMANCE) != 0u) {
        assessment->stage_bytes[SIXEL_ASSESSMENT_STAGE_IMAGE_DECODE] +=
            bytes;
        assessment->stage_bytes[SIXEL_ASSESSMENT_STAGE_SCALE] += bytes;
        assessment->stage_bytes[SIXEL_ASSESSMENT_STAGE_CROP] += bytes;
        assessment->stage_bytes[SIXEL_ASSESSMENT_STAGE_COLORSPACE] += bytes;
        assessment->stage_bytes[SIXEL_ASSESSMENT_STAGE_PALETTE_HISTOGRAM] +=
            bytes;
        assessment->stage_bytes[SIXEL_ASSESSMENT_STAGE_PALETTE_SOLVE] +=
            bytes;
        assessment->stage_bytes[SIXEL_ASSESSMENT_STAGE_PALETTE_APPLY] +=
            bytes;
        assessment->stage_bytes[SIXEL_ASSESSMENT_STAGE_ENCODE] += bytes;
    }
}

SIXELAPI void
sixel_assessment_record_output_size(sixel_assessment_t *assessment,
                                    size_t output_bytes)
{
    if (assessment == NULL) {
        return;
    }
    if (output_bytes == 0u && assessment->output_bytes_written > 0u) {
        output_bytes = assessment->output_bytes_written;
    }
    assessment->output_bytes = output_bytes;
    if ((assessment->sections_mask &
            SIXEL_ASSESSMENT_SECTION_PERFORMANCE) != 0u) {
        assessment->stage_bytes[SIXEL_ASSESSMENT_STAGE_OUTPUT] +=
            output_bytes;
    }
}

SIXELAPI void
sixel_assessment_record_output_write(sixel_assessment_t *assessment,
                                     size_t bytes,
                                     double duration)
{
    if (assessment == NULL) {
        return;
    }
    if (bytes > 0u) {
        assessment->output_bytes_written += bytes;
    }
    if (duration > 0.0 &&
            (assessment->sections_mask &
             SIXEL_ASSESSMENT_SECTION_PERFORMANCE) != 0u) {
        /* The output bucket collects every fn_write() span verbatim. */
        assessment->stage_durations[SIXEL_ASSESSMENT_STAGE_OUTPUT] += duration;
        if (assessment->active_stage == SIXEL_ASSESSMENT_STAGE_ENCODE) {
            assessment->encode_output_time_pending += duration;
        }
    }
}

SIXELAPI int
sixel_assessment_palette_probe_enabled(void)
{
    return g_active_encode_assessment != NULL;
}

SIXELAPI void
sixel_assessment_record_palette_apply_span(double duration)
{
    sixel_assessment_t *assessment;

    if (duration <= 0.0) {
        return;
    }
    assessment = g_active_encode_assessment;
    if (assessment == NULL) {
        return;
    }
    if ((assessment->sections_mask &
            SIXEL_ASSESSMENT_SECTION_PERFORMANCE) == 0u) {
        return;
    }
    /*
     * Palette work performed inside sixel_encode() belongs to the
     * palette bucket even though the call happens while the encode
     * stage is active.  The time goes straight to the palette stage
     * while we stash the same span for the later encode rebalance.
     */
    assessment->stage_durations[SIXEL_ASSESSMENT_STAGE_PALETTE_APPLY] +=
        duration;
    assessment->encode_palette_time_pending += duration;
}

SIXELAPI int
sixel_assessment_encode_probe_enabled(void)
{
    sixel_assessment_t *assessment;

    assessment = g_active_encode_assessment;
    if (assessment == NULL) {
        return 0;
    }
    if ((assessment->sections_mask &
            SIXEL_ASSESSMENT_SECTION_PERFORMANCE) == 0u) {
        return 0;
    }
    return 1;
}

SIXELAPI void
sixel_assessment_set_encode_parallelism(int threads)
{
    if (threads < 1) {
        threads = 1;
    }
    g_encode_parallel_threads = threads;
    if (threads > 1) {
        g_encode_substages_visible = 0;
    } else {
        g_encode_substages_visible = 1;
    }
}

SIXELAPI void
sixel_assessment_record_encode_span(sixel_assessment_stage_t stage,
                                    double duration)
{
    sixel_assessment_t *assessment;

    if (duration <= 0.0) {
        return;
    }
    if (stage <= SIXEL_ASSESSMENT_STAGE_NONE ||
            stage >= SIXEL_ASSESSMENT_STAGE_COUNT) {
        return;
    }
    assessment = g_active_encode_assessment;
    if (assessment == NULL) {
        return;
    }
    if ((assessment->sections_mask &
            SIXEL_ASSESSMENT_SECTION_PERFORMANCE) == 0u) {
        return;
    }
    assessment->stage_durations[stage] += duration;
}

SIXELAPI void
sixel_assessment_record_encode_work(sixel_assessment_stage_t stage,
                                    double amount)
{
    sixel_assessment_t *assessment;

    if (amount <= 0.0) {
        return;
    }
    if (stage <= SIXEL_ASSESSMENT_STAGE_NONE ||
            stage >= SIXEL_ASSESSMENT_STAGE_COUNT) {
        return;
    }
    assessment = g_active_encode_assessment;
    if (assessment == NULL) {
        return;
    }
    if ((assessment->sections_mask &
            SIXEL_ASSESSMENT_SECTION_PERFORMANCE) == 0u) {
        return;
    }
    assessment->stage_bytes[stage] += amount;
}

SIXELAPI void
sixel_assessment_select_sections(sixel_assessment_t *assessment,
                                 unsigned int sections)
{
    unsigned int section_mask;
    unsigned int view_mask;

    if (assessment == NULL) {
        return;
    }
    section_mask = sections & SIXEL_ASSESSMENT_SECTION_MASK;
    if ((section_mask & SIXEL_ASSESSMENT_SECTION_QUALITY) == 0u) {
        view_mask = SIXEL_ASSESSMENT_VIEW_ENCODED;
    } else if ((sections & SIXEL_ASSESSMENT_VIEW_MASK) != 0u) {
        view_mask = SIXEL_ASSESSMENT_VIEW_QUANTIZED;
    } else {
        view_mask = SIXEL_ASSESSMENT_VIEW_ENCODED;
    }
    assessment->sections_mask = section_mask;
    assessment->view_mask = view_mask;
    if ((section_mask & SIXEL_ASSESSMENT_SECTION_PERFORMANCE) == 0u) {
        assessment->stage_active = 0;
        if (g_active_encode_assessment == assessment) {
            g_active_encode_assessment = NULL;
        }
    }
}

SIXELAPI void
sixel_assessment_select_metrics(sixel_assessment_t *assessment,
                                unsigned int metrics)
{
    unsigned int mask;

    if (assessment == NULL) {
        return;
    }
    mask = metrics & assessment_metric_mask_all();
    if (mask == 0u) {
        mask = assessment_metric_mask_all();
    }
    assessment->metrics_mask = mask;
}

SIXELAPI void
sixel_assessment_attach_encoder(sixel_assessment_t *assessment,
                                sixel_encoder_t *encoder)
{
    (void)assessment;
    (void)encoder;
}

static void align_frame_pixels(float **ref_pixels,
                               int *ref_width,
                               int *ref_height,
                               float **out_pixels,
                               int *out_width,
                               int *out_height);

static void
assessment_fail(SIXELSTATUS status, char const *message)
{
    sixel_assessment_t *ctx;
    size_t length;

    ctx = g_assessment_context;
    if (ctx != NULL) {
        ctx->last_error = status;
        if (message != NULL) {
            length = strlen(message);
            if (length >= sizeof(ctx->error_message)) {
                length = sizeof(ctx->error_message) - 1u;
            }
            memcpy(ctx->error_message, message, length);
            ctx->error_message[length] = '\0';
        } else {
            ctx->error_message[0] = '\0';
        }
        longjmp(ctx->bailout, 1);
    }
    if (message != NULL) {
        fprintf(stderr, "%s\n", message);
    } else {
        fprintf(stderr, "assessment failure\n");
    }
    abort();
}

/*
 * Memory helpers
 */
static void *xmalloc(size_t size)
{
    void *ptr;
    ptr = malloc(size);
    if (ptr == NULL) {
        assessment_fail(SIXEL_BAD_ALLOCATION,
                       "malloc failed while building assessment state");
    }
    return ptr;
}

static void *xcalloc(size_t nmemb, size_t size)
{
    void *ptr;
    ptr = calloc(nmemb, size);
    if (ptr == NULL) {
        assessment_fail(SIXEL_BAD_ALLOCATION,
                       "calloc failed while building assessment state");
    }
    return ptr;
}

/*
 * Loader bridge (libsixel frame -> float triplets)
 */
static SIXELSTATUS copy_frame_to_rgb(sixel_frame_t *frame,
                                     unsigned char **pixels,
                                     int *width,
                                     int *height)
{
    SIXELSTATUS status;
    int frame_width;
    int frame_height;
    int pixelformat;
    size_t size;
    unsigned char *buffer;
    int normalized_format;

    frame_width = sixel_frame_get_width(frame);
    frame_height = sixel_frame_get_height(frame);
    status = sixel_frame_strip_alpha(frame, NULL);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    pixelformat = sixel_frame_get_pixelformat(frame);
    size = (size_t)frame_width * (size_t)frame_height * 3u;
    /*
     * Use malloc here because the loader's allocator enforces a strict
     * allocation ceiling that can reject large frames even on hosts with
     * sufficient RAM available.
     */
    buffer = (unsigned char *)malloc(size);
    if (buffer == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_RGB888) {
        memcpy(buffer, sixel_frame_get_pixels(frame), size);
    } else {
        normalized_format = pixelformat;
        status = sixel_helper_normalize_pixelformat(
            buffer,
            &normalized_format,
            sixel_frame_get_pixels(frame),
            pixelformat,
            frame_width,
            frame_height);
        if (SIXEL_FAILED(status)) {
            free(buffer);
            return status;
        }
    }
    *pixels = buffer;
    *width = frame_width;
    *height = frame_height;
    return SIXEL_OK;
}

static SIXELSTATUS
frame_to_rgb_float(sixel_frame_t *frame,
                   float **pixels_out,
                   int *width_out,
                   int *height_out)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    float const *float_pixels;
    int width;
    int height;
    int pixelformat;
    float *converted;
    size_t count;
    size_t index;
    sixel_frame_pixels_view_t view;

    status = SIXEL_FALSE;
    pixels = NULL;
    width = 0;
    height = 0;
    pixelformat = SIXEL_PIXELFORMAT_RGB888;
    float_pixels = NULL;
    converted = NULL;
    count = 0;
    index = 0;
    memset(&view, 0, sizeof(view));

    if (frame == NULL || pixels_out == NULL || width_out == NULL ||
            height_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *pixels_out = NULL;
    *width_out = 0;
    *height_out = 0;

    status = sixel_frame_strip_alpha(frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    pixelformat = sixel_frame_get_pixelformat(frame);
    count = (size_t)width * (size_t)height;
    if (height != 0 && count / (size_t)height != (size_t)width) {
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    if (count > SIZE_MAX / (size_t)SIXEL_ASSESSMENT_RGB_CHANNELS) {
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    count *= (size_t)SIXEL_ASSESSMENT_RGB_CHANNELS;
    converted = (float *)xmalloc(count * sizeof(float));
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        status = sixel_assessment_frame_get_pixels_view(frame, &view);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        float_pixels = view.pixels_float32;
        if (float_pixels == NULL) {
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }
        memcpy(converted, float_pixels, count * sizeof(float));
    } else {
        status = copy_frame_to_rgb(frame, &pixels, &width, &height);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        if (pixels == NULL) {
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }
        for (index = 0; index < count; ++index) {
            converted[index] = pixels[index] / 255.0f;
        }
        for (index = 0; index < count; index += 3) {
            if (sixel_frame_get_colorspace(frame) == SIXEL_COLORSPACE_OKLAB) {
                converted[index + 1] -= 0.5f;
                converted[index + 2] -= 0.5f;
            } else if (sixel_frame_get_colorspace(frame)
                           == SIXEL_COLORSPACE_CIELAB) {
                converted[index + 1] = (converted[index + 1] - 0.5f) * 3;
                converted[index + 2] = (converted[index + 2] - 0.5f) * 3;
            } else if (sixel_frame_get_colorspace(frame)
                           == SIXEL_COLORSPACE_DIN99D) {
                converted[index + 1] = (converted[index + 1] - 0.5f) * 2;
                converted[index + 2] = (converted[index + 2] - 0.5f) * 2;
            }
        }
    }
    *pixels_out = converted;
    *width_out = width;
    *height_out = height;
    converted = NULL;
    status = SIXEL_OK;

cleanup:
    if (pixels != NULL) {
        free(pixels);
    }
    if (converted != NULL) {
        free(converted);
    }
    return status;
}



/*
 * Array math helpers
 */
static sixel_assessment_float_buffer_t
float_buffer_create(size_t length)
{
    sixel_assessment_float_buffer_t buf;
    buf.length = length;
    buf.values = (float *)xcalloc(length, sizeof(float));
    return buf;
}

static void
float_buffer_free(sixel_assessment_float_buffer_t *buf)
{
    if (buf->values != NULL) {
        free(buf->values);
        buf->values = NULL;
        buf->length = 0;
    }
}

static float
clamp_float(float v, float min_v, float max_v)
{
    float result;
    result = v;
    if (result < min_v) {
        result = min_v;
    }
    if (result > max_v) {
        result = max_v;
    }
    return result;
}

/*
 * Luma conversion and resizing utilities
 */
#if defined(HAVE_DIAGNOSTIC_WANALYZER_USE_OF_UNINITIALIZED_VALUE) && \
    defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
#endif
static sixel_assessment_float_buffer_t
pixels_to_luma709(const float *pixels, int width, int height)
{
    sixel_assessment_float_buffer_t buf;
    size_t total;
    size_t i;
    float r;
    float g;
    float b;

    total = (size_t)width * (size_t)height;
    buf = float_buffer_create(total);
    if (pixels == NULL || buf.values == NULL) {
        return buf;
    }
    for (i = 0; i < total; ++i) {
        r = pixels[i * 3 + 0];
        g = pixels[i * 3 + 1];
        b = pixels[i * 3 + 2];
        buf.values[i] = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }
    return buf;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_USE_OF_UNINITIALIZED_VALUE) && \
    defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

static sixel_assessment_float_buffer_t
pixels_channel(const float *pixels, int width, int height, int channel)
{
    sixel_assessment_float_buffer_t buf;
    size_t total;
    size_t i;
    float value;

    total = (size_t)width * (size_t)height;
    buf = float_buffer_create(total);
    if (pixels == NULL || buf.values == NULL) {
        return buf;
    }
    for (i = 0; i < total; ++i) {
        value = pixels[i * 3 + channel];
        buf.values[i] = value;
    }
    return buf;
}
/*
 * Gaussian kernel and separable convolution
 */
static sixel_assessment_float_buffer_t gaussian_kernel1d(int size, float sigma)
{
    sixel_assessment_float_buffer_t kernel;
    int i;
    float mean;
    float sum;
    float x;
    float value;

    kernel = float_buffer_create((size_t)size);
    mean = ((float)size - 1.0f) * 0.5f;
    sum = 0.0f;
    for (i = 0; i < size; ++i) {
        x = (float)i - mean;
        value = expf(-0.5f * (x / sigma) * (x / sigma));
        kernel.values[i] = value;
        sum += value;
    }
    if (sum > 0.0f) {
        for (i = 0; i < size; ++i) {
            kernel.values[i] /= sum;
        }
    }
    return kernel;
}

static sixel_assessment_float_buffer_t
separable_conv2d(const sixel_assessment_float_buffer_t *img, int width,
                 int height, const sixel_assessment_float_buffer_t *kernel)
{
    sixel_assessment_float_buffer_t tmp;
    sixel_assessment_float_buffer_t out;
    int pad;
    int x;
    int y;
    int k;
    int kernel_size;
    float acc;
    int px;
    int py;
    int idx;
    float sample;

    pad = (int)kernel->length / 2;
    kernel_size = (int)kernel->length;
    tmp = float_buffer_create((size_t)width * (size_t)height);
    out = float_buffer_create((size_t)width * (size_t)height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            acc = 0.0f;
            for (k = 0; k < kernel_size; ++k) {
                px = x + k - pad;
                if (px < 0) {
                    px = -px;
                }
                if (px >= width) {
                    px = width - (px - width) - 1;
                    if (px < 0) {
                        px = 0;
                    }
                }
                idx = y * width + px;
                sample = img->values[idx];
                acc += kernel->values[k] * sample;
            }
            tmp.values[y * width + x] = acc;
        }
    }

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            acc = 0.0f;
            for (k = 0; k < kernel_size; ++k) {
                py = y + k - pad;
                if (py < 0) {
                    py = -py;
                }
                if (py >= height) {
                    py = height - (py - height) - 1;
                    if (py < 0) {
                        py = 0;
                    }
                }
                idx = py * width + x;
                sample = tmp.values[idx];
                acc += kernel->values[k] * sample;
            }
            out.values[y * width + x] = acc;
        }
    }

    float_buffer_free(&tmp);
    return out;
}

/* Wang's five-scale reference uses valid Gaussian windows and the product
 * of mean contrast/structure at scales 1--4 and mean full SSIM at scale 5.
 * Double moments avoid cancellation in flat patches and double pooling
 * avoids image-size-dependent float summation error. */
static double
ms_ssim_scale(double const *a, double const *b, double *horizontal,
              int width, int height, double const kernel[11], int last)
{
    double moments[5];
    double const *window_rows[11];
    double av;
    double bv;
    double weight;
    double cs;
    double denominator;
    double sum;
    size_t source;
    size_t target;
    int valid_width;
    int row;
    int column;
    int tap;
    int moment;

    valid_width = width - 10;
    sum = 0.0;
    for (row = 0; row < height; ++row) {
        /* A vertical window consumes only eleven horizontal rows. Replace
         * the oldest row, then emit the next complete window immediately
         * to keep the working set bounded. No row is read before
         * it is filled, including when this storage is reused at a scale. */
        for (column = 0; column < valid_width; ++column) {
            memset(moments, 0, sizeof(moments));
            source = (size_t)row * (size_t)width + (size_t)column;
            for (tap = 0; tap < 11; ++tap) {
                av = a[source + (size_t)tap];
                bv = b[source + (size_t)tap];
                weight = kernel[tap];
                moments[0] += weight * av;
                moments[1] += weight * bv;
                moments[2] += weight * av * av;
                moments[3] += weight * bv * bv;
                moments[4] += weight * av * bv;
            }
            target = ((size_t)(row % 11) * (size_t)valid_width +
                (size_t)column) * 5u;
            memcpy(horizontal + target, moments, sizeof(moments));
        }
        if (row < 10) {
            continue;
        }
        /* Resolve ring positions once per output row, not per pixel/tap.
         * Keep tap order and row-major pooling identical to the full-image
         * implementation: changing reduction order changes the metric. */
        for (tap = 0; tap < 11; ++tap) {
            window_rows[tap] = horizontal
                + (size_t)((row - 10 + tap) % 11)
                * (size_t)valid_width * 5u;
        }
        for (column = 0; column < valid_width; ++column) {
            memset(moments, 0, sizeof(moments));
            source = (size_t)column * 5u;
            for (tap = 0; tap < 11; ++tap) {
                for (moment = 0; moment < 5; ++moment) {
                    moments[moment] += kernel[tap] *
                        window_rows[tap][source + (size_t)moment];
                }
            }
            av = moments[0];
            bv = moments[1];
            denominator = moments[2] - av * av + moments[3] - bv * bv
                + 0.0009;
            if (!(denominator > 0.0)) {
                return NAN;
            }
            cs = (2.0 * (moments[4] - av * bv) + 0.0009) / denominator;
            if (last) {
                cs *= (2.0 * av * bv + 0.0001) /
                    (av * av + bv * bv + 0.0001);
            }
            sum += cs;
        }
    }
    return sum / ((double)valid_width * (double)(height - 10));
}

/* Match imfilter(..., ones(2)/4, 'symmetric', 'same')(1:2:end,...):
 * the even-sized correlation window looks right/down. An odd final row or
 * column is reflected onto itself, retaining ceil(size/2) samples. Earlier
 * compacted outputs never overwrite an input still needed by this pass. */
static void
ms_ssim_downsample(double *pixels, int width, int height)
{
    int row;
    int column;
    int next_row;
    int next_column;
    size_t first;
    size_t second;
    size_t output;

    output = 0u;
    for (row = 0; row < height; row += 2) {
        next_row = row + 1 < height ? row + 1 : row;
        first = (size_t)row * (size_t)width;
        second = (size_t)next_row * (size_t)width;
        for (column = 0; column < width; column += 2) {
            next_column = column + 1 < width ? column + 1 : column;
            pixels[output++] = 0.25 *
                (pixels[first + (size_t)column] +
                 pixels[first + (size_t)next_column] +
                 pixels[second + (size_t)column] +
                 pixels[second + (size_t)next_column]);
        }
    }
}

static float
ms_ssim_luma(sixel_assessment_float_buffer_t const *ref,
                    sixel_assessment_float_buffer_t const *out,
                    int width, int height)
{
    double const weights[5] = {0.0448, 0.2856, 0.3001, 0.2363, 0.1333};
    double kernel[11];
    double kernel_sum;
    double factor;
    double product;
    double *workspace;
    double *a;
    double *b;
    double *horizontal;
    size_t count;
    size_t horizontal_count;
    size_t index;
    int tap;
    int level;

    /* Do not silently replace five scales with a different small-image
     * metric. Follow the reference's 11 * 2^4 admission rule. */
    if (width < 176 || height < 176) {
        return NAN;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        assessment_fail(SIXEL_BAD_ALLOCATION,
                        "MS-SSIM image size overflow");
    }
    count = (size_t)width * (size_t)height;
    /* Retain the conservative bound for two images plus five moments per
     * pixel. With height >= 176, the eleven-row ring is strictly smaller
     * than the old five-image scratch area, so the allocation below fits. */
    if (count > SIZE_MAX / (7u * sizeof(double))) {
        assessment_fail(SIXEL_BAD_ALLOCATION,
                        "MS-SSIM workspace size overflow");
    }
    for (index = 0u; index < count; ++index) {
        if (!isfinite(ref->values[index]) || !isfinite(out->values[index])) {
            return NAN;
        }
    }
    horizontal_count = (size_t)(width - 10) * 11u * 5u;
    workspace = (double *)xmalloc(
        (count * 2u + horizontal_count) * sizeof(double));
    a = workspace;
    b = a + count;
    horizontal = b + count;
    for (index = 0u; index < count; ++index) {
        a[index] = ref->values[index];
        b[index] = out->values[index];
    }
    kernel_sum = 0.0;
    for (tap = 0; tap < 11; ++tap) {
        kernel[tap] = exp(-(double)((tap - 5) * (tap - 5)) / 4.5);
        kernel_sum += kernel[tap];
    }
    for (tap = 0; tap < 11; ++tap) {
        kernel[tap] /= kernel_sum;
    }
    product = 1.0;
    for (level = 0; level < 5; ++level) {
        factor = ms_ssim_scale(a, b, horizontal, width, height,
                               kernel, level == 4);
        /* Fractional powers of negative mean factors are not real-valued.
         * Report an unavailable metric, not a silently clipped substitute.
         * Tiny overshoot above one is only bounded at the final result. */
        if (!isfinite(factor) || factor < 0.0) {
            product = NAN;
            break;
        }
        product *= pow(factor, weights[level]);
        if (level < 4) {
            ms_ssim_downsample(a, width, height);
            ms_ssim_downsample(b, width, height);
            width = width / 2 + width % 2;
            height = height / 2 + height % 2;
        }
    }
    free(workspace);
    return (float)(product > 1.0 ? 1.0 : product);
}
/* Pixel-domain VIF uses four valid Gaussian windows (17, 9, 5, 3), with
 * sigma = window / 5. At scales 2--4, filter with that scale's window and
 * retain samples 0, 2, ... before computing local moments. These helpers
 * implement the scalar model, not the wavelet/vector VIF formulation. */
static void
vif_p_downsample(double *pixels, double *horizontal, int width, int height,
                 double const *kernel, int size)
{
    double sum;
    size_t source;
    size_t target;
    int valid_width;
    int row;
    int column;
    int tap;

    valid_width = width - size + 1;
    for (row = 0; row < height; ++row) {
        for (column = 0; column < valid_width; ++column) {
            source = (size_t)row * (size_t)width + (size_t)column;
            sum = 0.0;
            for (tap = 0; tap < size; ++tap) {
                sum += kernel[tap] * pixels[source + (size_t)tap];
            }
            horizontal[(size_t)row * (size_t)valid_width +
                (size_t)column] = sum;
        }
    }
    /* Every horizontal input is saved before compacting pixels in place. */
    target = 0u;
    for (row = 0; row <= height - size; row += 2) {
        for (column = 0; column < valid_width; column += 2) {
            sum = 0.0;
            for (tap = 0; tap < size; ++tap) {
                source = (size_t)(row + tap) * (size_t)valid_width +
                    (size_t)column;
                sum += kernel[tap] * horizontal[source];
            }
            pixels[target++] = sum;
        }
    }
}

static void
vif_p_scale(double const *a, double const *b, double *horizontal,
             int width, int height, double const *kernel, int size,
             double *numerator, double *denominator)
{
    double const epsilon = 1e-10;
    double moments[5];
    double const *window_rows[17];
    double av;
    double bv;
    double weight;
    double variance_a;
    double variance_b;
    double covariance;
    double gain;
    double residual;
    size_t source;
    size_t target;
    int valid_width;
    int row;
    int column;
    int tap;
    int moment;

    valid_width = width - size + 1;
    for (row = 0; row < height; ++row) {
        /* Retain only the horizontal rows in the current vertical window.
         * Double moments limit cancellation for nearly uniform patches. */
        for (column = 0; column < valid_width; ++column) {
            memset(moments, 0, sizeof(moments));
            source = (size_t)row * (size_t)width + (size_t)column;
            for (tap = 0; tap < size; ++tap) {
                av = a[source + (size_t)tap];
                bv = b[source + (size_t)tap];
                weight = kernel[tap];
                moments[0] += weight * av;
                moments[1] += weight * bv;
                moments[2] += weight * av * av;
                moments[3] += weight * bv * bv;
                moments[4] += weight * av * bv;
            }
            target = ((size_t)(row % size) * (size_t)valid_width +
                (size_t)column) * 5u;
            memcpy(horizontal + target, moments, sizeof(moments));
        }
        if (row < size - 1) {
            continue;
        }
        for (tap = 0; tap < size; ++tap) {
            window_rows[tap] = horizontal +
                (size_t)((row - size + 1 + tap) % size) *
                (size_t)valid_width * 5u;
        }
        for (column = 0; column < valid_width; ++column) {
            memset(moments, 0, sizeof(moments));
            source = (size_t)column * 5u;
            for (tap = 0; tap < size; ++tap) {
                for (moment = 0; moment < 5; ++moment) {
                    moments[moment] += kernel[tap] *
                        window_rows[tap][source + (size_t)moment];
                }
            }
            av = moments[0];
            bv = moments[1];
            variance_a = fmax(0.0, moments[2] - av * av);
            variance_b = fmax(0.0, moments[3] - bv * bv);
            covariance = moments[4] - av * bv;
            gain = covariance / (variance_a + epsilon);
            residual = variance_b - gain * covariance;
            /* Flat reference patches carry no information. Flat targets
             * and negative correlations cannot transmit reference detail.
             * Keep these corrections in the reference algorithm's order. */
            if (variance_a < epsilon) {
                gain = 0.0;
                residual = variance_b;
                variance_a = 0.0;
            }
            if (variance_b < epsilon) {
                gain = 0.0;
                residual = 0.0;
            }
            if (gain < 0.0) {
                residual = variance_b;
                gain = 0.0;
            }
            residual = fmax(residual, epsilon);
            /* Neural noise variance is 2 in the 0--255 signal domain.
             * Pool information across scales before taking the ratio;
             * averaging four individual ratios defines a different score. */
            *numerator += log1p(gain * gain * variance_a /
                               (residual + 2.0));
            *denominator += log1p(variance_a / 2.0);
        }
    }
}

static float
vif_p_luma(sixel_assessment_float_buffer_t const *ref,
            sixel_assessment_float_buffer_t const *out,
            int width, int height)
{
    double kernel[17];
    double sum;
    double coordinate;
    double numerator;
    double denominator;
    double *workspace;
    double *a;
    double *b;
    double *horizontal;
    size_t count;
    size_t scratch;
    size_t index;
    int level;
    int size;
    int tap;

    /* 41 becomes 17, 7, 3 after valid filtering and subsampling. Anything
     * smaller lacks a complete final window; do not reduce the scale count. */
    if (width < 41 || height < 41) {
        return NAN;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        assessment_fail(SIXEL_BAD_ALLOCATION, "VIF-P image size overflow");
    }
    count = (size_t)width * (size_t)height;
    /* Two images plus scratch: max(one image, 17 rows of five moments).
     * Height >= 41 makes the scratch strictly smaller than three images. */
    if (count > SIZE_MAX / (5u * sizeof(double))) {
        assessment_fail(SIXEL_BAD_ALLOCATION,
                        "VIF-P workspace size overflow");
    }
    for (index = 0u; index < count; ++index) {
        if (!isfinite(ref->values[index]) || !isfinite(out->values[index])) {
            return NAN;
        }
    }
    scratch = (size_t)(width - 16) * 17u * 5u;
    if (scratch < count) {
        scratch = count;
    }
    workspace = (double *)xmalloc((2u * count + scratch) * sizeof(double));
    a = workspace;
    b = a + count;
    horizontal = b + count;
    for (index = 0u; index < count; ++index) {
        a[index] = (double)ref->values[index] * 255.0;
        b[index] = (double)out->values[index] * 255.0;
    }
    numerator = 0.0;
    denominator = 0.0;
    for (level = 0; level < 4; ++level) {
        size = (1 << (4 - level)) + 1;
        sum = 0.0;
        for (tap = 0; tap < size; ++tap) {
            coordinate = (double)(tap - size / 2) / ((double)size / 5.0);
            kernel[tap] = exp(-0.5 * coordinate * coordinate);
            sum += kernel[tap];
        }
        for (tap = 0; tap < size; ++tap) {
            kernel[tap] /= sum;
        }
        if (level != 0) {
            vif_p_downsample(a, horizontal, width, height, kernel, size);
            vif_p_downsample(b, horizontal, width, height, kernel, size);
            width = (width - size + 2) / 2;
            height = (height - size + 2) / 2;
        }
        vif_p_scale(a, b, horizontal, width, height, kernel, size,
                     &numerator, &denominator);
    }
    free(workspace);
    /* An information-free reference makes the ratio undefined, even for
     * identity. Preserve gains above one for contrast-enhanced targets. */
    return denominator > 0.0 ? (float)(numerator / denominator) : NAN;
}

/*
 * FFT helpers for spectral metrics
 */
static int
next_power_of_two(int value)
{
    int n;
    n = 1;
    while (n < value) {
        n <<= 1;
    }
    return n;
}

static void
fft_bit_reverse(sixel_assessment_complex_t *data, int n)
{
    int i;
    int j;
    int bit;
    sixel_assessment_complex_t tmp;

    j = 0;
    for (i = 0; i < n; ++i) {
        if (i < j) {
            tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
        }
        bit = n >> 1;
        while ((j & bit) != 0) {
            j &= ~bit;
            bit >>= 1;
        }
        j |= bit;
    }
}

static void
fft_cooley_tukey(sixel_assessment_complex_t *data, int n, int inverse)
{
    int len;
    double angle;
    sixel_assessment_complex_t wlen;
    int half;
    int i;
    sixel_assessment_complex_t w;
    int j;
    sixel_assessment_complex_t u;
    sixel_assessment_complex_t v;
    double tmp_re;
    double tmp_im;

    if (data == NULL || n <= 1) {
        return;
    }

    fft_bit_reverse(data, n);
    for (len = 2; len <= n; len <<= 1) {

        angle = 2.0 * M_PI / (double)len;
        if (inverse) {
            angle = -angle;
        }
        wlen.re = cos(angle);
        wlen.im = sin(angle);
        half = len >> 1;
        for (i = 0; i < n; i += len) {
            w.re = 1.0;
            w.im = 0.0;
            for (j = 0; j < half; ++j) {
                int left;
                int right;
                sixel_assessment_complex_t t;

                left = i + j;
                right = left + half;
                if (right >= n) {
                    break;
                }
                u = data[left];
                t = data[right];
                v.re = t.re * w.re - t.im * w.im;
                v.im = t.re * w.im + t.im * w.re;
                data[left].re = u.re + v.re;
                data[left].im = u.im + v.im;
                data[right].re = u.re - v.re;
                data[right].im = u.im - v.im;
                tmp_re = w.re * wlen.re - w.im * wlen.im;
                tmp_im = w.re * wlen.im + w.im * wlen.re;
                w.re = tmp_re;
                w.im = tmp_im;
            }
        }
    }
    if (inverse) {
        for (i = 0; i < n; ++i) {
            data[i].re /= n;
            data[i].im /= n;
        }
    }
}

static void
fft2d(sixel_assessment_float_buffer_t *input, int width, int height,
      sixel_assessment_complex_t *output, int out_width, int out_height)
{
    int padded_width;
    int padded_height;
    int y;
    int x;
    sixel_assessment_complex_t *row;
    sixel_assessment_complex_t *col;
    sixel_assessment_complex_t value;

    padded_width = out_width;
    padded_height = out_height;
    row = (sixel_assessment_complex_t *)xmalloc(
        sizeof(sixel_assessment_complex_t) * (size_t)padded_width);
    col = (sixel_assessment_complex_t *)xmalloc(
        sizeof(sixel_assessment_complex_t) * (size_t)padded_height);

    for (y = 0; y < padded_height; ++y) {
        for (x = 0; x < padded_width; ++x) {
            if (y < height && x < width) {
                value.re = input->values[y * width + x];
                value.im = 0.0;
            } else {
                value.re = 0.0;
                value.im = 0.0;
            }
            output[y * padded_width + x] = value;
        }
    }

    for (y = 0; y < padded_height; ++y) {
        for (x = 0; x < padded_width; ++x) {
            row[x] = output[y * padded_width + x];
        }
        fft_cooley_tukey(row, padded_width, 0);
        for (x = 0; x < padded_width; ++x) {
            output[y * padded_width + x] = row[x];
        }
    }

    for (x = 0; x < padded_width; ++x) {
        for (y = 0; y < padded_height; ++y) {
            col[y] = output[y * padded_width + x];
        }
        fft_cooley_tukey(col, padded_height, 0);
        for (y = 0; y < padded_height; ++y) {
            output[y * padded_width + x] = col[y];
        }
    }

    free(row);
    free(col);
}

static void
fft_shift(sixel_assessment_complex_t *data, int width, int height)
{
    int half_w;
    int half_h;
    int y;
    int x;
    int nx;
    int ny;
    sixel_assessment_complex_t tmp;

    half_w = width / 2;
    half_h = height / 2;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            nx = (x + half_w) % width;
            ny = (y + half_h) % height;
            if (ny > y || (ny == y && nx > x)) {
                continue;
            }
            tmp = data[y * width + x];
            data[y * width + x] = data[ny * width + nx];
            data[ny * width + nx] = tmp;
        }
    }
}
static float
high_frequency_ratio(const sixel_assessment_float_buffer_t *img,
                     int width, int height, float cutoff)
{
    int padded_width;
    int padded_height;
    sixel_assessment_complex_t *freq;
    size_t total;
    sixel_assessment_float_buffer_t centered;
    double hi_sum;
    double total_sum;
    int y;
    int x;
    double cy;
    double cx;
    double mean;
    size_t i;
    double dy;
    double dx;
    double r;
    double norm;
    double power;

    padded_width = next_power_of_two(width);
    padded_height = next_power_of_two(height);
    total = (size_t)padded_width * (size_t)padded_height;
    freq = (sixel_assessment_complex_t *)xmalloc(
        total * sizeof(sixel_assessment_complex_t));
    centered = float_buffer_create((size_t)width * (size_t)height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            centered.values[y * width + x] = img->values[y * width + x];
        }
    }
    mean = 0.0;
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        mean += centered.values[i];
    }
    mean /= (double)((size_t)width * (size_t)height);
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        centered.values[i] -= (float)mean;
    }

    fft2d(&centered, width, height, freq, padded_width, padded_height);
    fft_shift(freq, padded_width, padded_height);

    hi_sum = 0.0;
    total_sum = 0.0;
    cy = padded_height / 2.0;
    cx = padded_width / 2.0;

    for (y = 0; y < padded_height; ++y) {
        for (x = 0; x < padded_width; ++x) {
            dy = (double)y - cy;
            dx = (double)x - cx;
            r = sqrt(dy * dy + dx * dx);
            norm = r / (0.5 * sqrt((double)padded_height *
                                   (double)padded_height +
                                   (double)padded_width *
                                   (double)padded_width));
            power = freq[y * padded_width + x].re *
                    freq[y * padded_width + x].re +
                    freq[y * padded_width + x].im *
                    freq[y * padded_width + x].im;
            total_sum += power;
            if (norm >= cutoff) {
                hi_sum += power;
            }
        }
    }

    free(freq);
    float_buffer_free(&centered);

    if (total_sum <= 0.0) {
        return 0.0f;
    }
    return (float)(hi_sum / total_sum);
}

static float
stripe_score(const sixel_assessment_float_buffer_t *img, int width, int height,
             int bins)
{
    int padded_width;
    int padded_height;
    sixel_assessment_complex_t *freq;
    sixel_assessment_float_buffer_t centered;
    double cy;
    double cx;
    double rmin;
    double *hist;
    int y;
    int x;
    double mean_val;
    double max_val;
    double mean;
    size_t i;
    double dy;
    double dx;
    double r;
    double ang;
    int index;
    double power;

    padded_width = next_power_of_two(width);
    padded_height = next_power_of_two(height);
    freq = (sixel_assessment_complex_t *)xmalloc(
        sizeof(sixel_assessment_complex_t) *
        (size_t)padded_width * (size_t)padded_height);
    centered = float_buffer_create((size_t)width * (size_t)height);
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            centered.values[y * width + x] = img->values[y * width + x];
        }
    }
    mean = 0.0;
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        mean += centered.values[i];
    }
    mean /= (double)((size_t)width * (size_t)height);
    for (i = 0; i < (size_t)width * (size_t)height; ++i) {
        centered.values[i] -= (float)mean;
    }

    fft2d(&centered, width, height, freq, padded_width, padded_height);
    fft_shift(freq, padded_width, padded_height);

    hist = (double *)xcalloc((size_t)bins, sizeof(double));
    cy = padded_height / 2.0;
    cx = padded_width / 2.0;
    rmin = 0.05 * (double)(width > height ? width : height);

    for (y = 0; y < padded_height; ++y) {
        for (x = 0; x < padded_width; ++x) {
            dy = (double)y - cy;
            dx = (double)x - cx;
            r = sqrt(dy * dy + dx * dx);
            if (r < rmin) {
                continue;
            }
            ang = atan2(dy, dx);
            if (ang < 0.0) {
                ang += M_PI;
            }
            index = (int)(ang / M_PI * bins);
            if (index < 0) {
                index = 0;
            }
            if (index >= bins) {
                index = bins - 1;
            }
            power = freq[y * padded_width + x].re *
                    freq[y * padded_width + x].re +
                    freq[y * padded_width + x].im *
                    freq[y * padded_width + x].im;
            hist[index] += power;
        }
    }

    mean_val = 0.0;
    for (x = 0; x < bins; ++x) {
        mean_val += hist[x];
    }
    mean_val = mean_val / (double)bins + 1e-12;

    max_val = hist[0];
    for (x = 1; x < bins; ++x) {
        if (hist[x] > max_val) {
            max_val = hist[x];
        }
    }

    free(hist);
    free(freq);
    float_buffer_free(&centered);

    return (float)(max_val / mean_val);
}
/*
 * Banding metrics (run-length and gradient-based)
 */
static float
banding_index_runlen(const sixel_assessment_float_buffer_t *img,
                     int width, int height, int levels)
{
    int y;
    int x;
    double total_runs;
    double total_segments;
    int prev;
    int run_len;
    int segs;
    int runs_sum;
    int value;

    total_runs = 0.0;
    total_segments = 0.0;
    for (y = 0; y < height; ++y) {
        prev = (int)clamp_float(img->values[y * width] * (levels - 1) + 0.5f,
                                0.0f, (float)(levels - 1));
        run_len = 1;
        segs = 0;
        runs_sum = 0;
        for (x = 1; x < width; ++x) {
            value = (int)clamp_float(
                img->values[y * width + x] * (levels - 1) + 0.5f,
                0.0f, (float)(levels - 1));
            if (value == prev) {
                run_len += 1;
            } else {
                runs_sum += run_len;
                segs += 1;
                run_len = 1;
                prev = value;
            }
        }
        runs_sum += run_len;
        segs += 1;
        total_runs += (double)runs_sum / (double)segs;
        total_segments += 1.0;
    }
    if (total_segments <= 0.0) {
        return 0.0f;
    }
    return (float)((total_runs / total_segments) / (double)width);
}

static sixel_assessment_float_buffer_t
gaussian_blur(const sixel_assessment_float_buffer_t *img, int width,
              int height, float sigma, int ksize)
{
    sixel_assessment_float_buffer_t kernel;
    sixel_assessment_float_buffer_t blurred;

    kernel = gaussian_kernel1d(ksize, sigma);
    blurred = separable_conv2d(img, width, height, &kernel);
    float_buffer_free(&kernel);
    return blurred;
}

static void
finite_diff(const sixel_assessment_float_buffer_t *img,
            int width,
            int height,
            sixel_assessment_float_buffer_t *dx,
            sixel_assessment_float_buffer_t *dy)
{
    int x;
    int y;
    int xm1;
    int xp1;
    int ym1;
    int yp1;
    float vxm1;
    float vxp1;
    float vym1;
    float vyp1;

    *dx = float_buffer_create((size_t)width * (size_t)height);
    *dy = float_buffer_create((size_t)width * (size_t)height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            xm1 = x - 1;
            xp1 = x + 1;
            ym1 = y - 1;
            yp1 = y + 1;
            if (xm1 < 0) {
                xm1 = 0;
            }
            if (xp1 >= width) {
                xp1 = width - 1;
            }
            if (ym1 < 0) {
                ym1 = 0;
            }
            if (yp1 >= height) {
                yp1 = height - 1;
            }
            vxm1 = img->values[y * width + xm1];
            vxp1 = img->values[y * width + xp1];
            vym1 = img->values[ym1 * width + x];
            vyp1 = img->values[yp1 * width + x];
            dx->values[y * width + x] = (vxp1 - vxm1) * 0.5f;
            dy->values[y * width + x] = (vyp1 - vym1) * 0.5f;
        }
    }
}

static int
compare_floats(const void *a, const void *b)
{
    float fa;
    float fb;
    fa = *(const float *)a;
    fb = *(const float *)b;
    if (fa < fb) {
        return -1;
    }
    if (fa > fb) {
        return 1;
    }
    return 0;
}

static float
banding_index_gradient(const sixel_assessment_float_buffer_t *img,
                       int width, int height)
{
    sixel_assessment_float_buffer_t blurred;
    sixel_assessment_float_buffer_t dx;
    sixel_assessment_float_buffer_t dy;
    sixel_assessment_float_buffer_t grad;
    size_t total;
    float *sorted;
    double g99;
    int bins;
    double *hist;
    double *centers;
    int half;
    int i;
    double sum_hist;
    double sum_residual;
    double zero_thresh;
    double zero_mass;
    double sum_x;
    double sum_y;
    double sum_xx;
    double sum_xy;
    int count;
    double slope;
    double intercept;
    double gx;
    double gy;
    size_t idx_pos;
    double value;
    int bin_index;
    double xval;
    double yval;
    double denom;
    double env;
    double resid;

    blurred = gaussian_blur(img, width, height, 1.0f, 7);
    finite_diff(&blurred, width, height, &dx, &dy);
    grad = float_buffer_create((size_t)width * (size_t)height);

    total = (size_t)width * (size_t)height;
    for (i = 0; (size_t)i < total; ++i) {
        gx = dx.values[i];
        gy = dy.values[i];
        grad.values[i] = (float)sqrt(gx * gx + gy * gy);
    }

    sorted = (float *)xmalloc(total * sizeof(float));
    memcpy(sorted, grad.values, total * sizeof(float));
    qsort(sorted, total, sizeof(float), compare_floats);
    if (total == 0) {
        g99 = 0.0;
    } else {
        idx_pos = (size_t)((double)(total - 1) * 0.995);
        g99 = sorted[idx_pos];
    }
    g99 += 1e-9;
    free(sorted);

    bins = 128;
    hist = (double *)xcalloc((size_t)bins, sizeof(double));
    centers = (double *)xmalloc((size_t)bins * sizeof(double));
    for (i = 0; i < bins; ++i) {
        centers[i] = ((double)i + 0.5) * (g99 / (double)bins);
    }

    for (i = 0; (size_t)i < total; ++i) {
        if (grad.values[i] > (float)g99) {
            grad.values[i] = (float)g99;
        }
        value = grad.values[i];
        bin_index = (int)(value / g99 * bins);
        if (bin_index < 0) {
            bin_index = 0;
        }
        if (bin_index >= bins) {
            bin_index = bins - 1;
        }
        hist[bin_index] += 1.0;
    }

    for (i = 0; i < bins; ++i) {
        hist[i] += 1e-12;
    }

    half = bins / 2;
    sum_x = 0.0;
    sum_y = 0.0;
    sum_xx = 0.0;
    sum_xy = 0.0;
    count = bins - half;
    for (i = half; i < bins; ++i) {
        xval = centers[i];
        yval = log(hist[i]);
        sum_x += xval;
        sum_y += yval;
        sum_xx += xval * xval;
        sum_xy += xval * yval;
    }
    slope = 0.0;
    intercept = 0.0;
    if (count > 1) {
        denom = (double)count * sum_xx - sum_x * sum_x;
        if (fabs(denom) > 1e-12) {
            slope = ((double)count * sum_xy - sum_x * sum_y) / denom;
            intercept = (sum_y - slope * sum_x) / (double)count;
        }
    }

    sum_hist = 0.0;
    sum_residual = 0.0;
    for (i = 0; i < bins; ++i) {
        env = exp(intercept + slope * centers[i]);
        resid = hist[i] - env;
        if (resid < 0.0) {
            resid = 0.0;
        }
        sum_hist += hist[i];
        sum_residual += resid;
    }

    zero_thresh = 0.01 * g99;
    zero_mass = 0.0;
    if (total > 0) {
        for (i = 0; (size_t)i < total; ++i) {
            if (grad.values[i] <= (float)zero_thresh) {
                zero_mass += 1.0;
            }
        }
        zero_mass /= (double)total;
    }

    float_buffer_free(&blurred);
    float_buffer_free(&dx);
    float_buffer_free(&dy);
    float_buffer_free(&grad);
    free(hist);
    free(centers);

    if (sum_hist <= 0.0) {
        return 0.0f;
    }
    return (float)(0.6 * (sum_residual / sum_hist) + 0.4 * zero_mass);
}
/*
 * Clipping statistics
 */
static void clipping_rates(const float *pixels,
                           int width,
                           int height,
                           float *clip_l,
                           float *clip_r,
                           float *clip_g,
                           float *clip_b)
{
    sixel_assessment_float_buffer_t luma;
    sixel_assessment_float_buffer_t rch;
    sixel_assessment_float_buffer_t gch;
    sixel_assessment_float_buffer_t bch;
    size_t total;
    size_t i;
    double eps;
    double lo;
    double hi;

    luma = pixels_to_luma709(pixels, width, height);
    rch = pixels_channel(pixels, width, height, 0);
    gch = pixels_channel(pixels, width, height, 1);
    bch = pixels_channel(pixels, width, height, 2);
    total = (size_t)width * (size_t)height;
    eps = 1e-6;

    lo = 0.0;
    hi = 0.0;
    for (i = 0; i < total; ++i) {
        if (luma.values[i] <= eps) {
            lo += 1.0;
        }
        if (luma.values[i] >= 1.0 - eps) {
            hi += 1.0;
        }
    }
    *clip_l = (float)((lo + hi) / (double)total);

    lo = hi = 0.0;
    for (i = 0; i < total; ++i) {
        if (rch.values[i] <= eps) {
            lo += 1.0;
        }
        if (rch.values[i] >= 1.0 - eps) {
            hi += 1.0;
        }
    }
    *clip_r = (float)((lo + hi) / (double)total);

    lo = hi = 0.0;
    for (i = 0; i < total; ++i) {
        if (gch.values[i] <= eps) {
            lo += 1.0;
        }
        if (gch.values[i] >= 1.0 - eps) {
            hi += 1.0;
        }
    }
    *clip_g = (float)((lo + hi) / (double)total);

    lo = hi = 0.0;
    for (i = 0; i < total; ++i) {
        if (bch.values[i] <= eps) {
            lo += 1.0;
        }
        if (bch.values[i] >= 1.0 - eps) {
            hi += 1.0;
        }
    }
    *clip_b = (float)((lo + hi) / (double)total);

    float_buffer_free(&luma);
    float_buffer_free(&rch);
    float_buffer_free(&gch);
    float_buffer_free(&bch);
}

/* XYZ already has the common D50 white; Lab uses ordinary CIE units. */
static sixel_assessment_float_buffer_t
pcs_to_lab(const double *xyz, int width, int height)
{
    sixel_assessment_float_buffer_t lab;
    size_t i;
    size_t count;
    int c;
    double value[3];

    count = (size_t)width * (size_t)height;
    lab = float_buffer_create(count * 3);
    for (i = 0; i < count; ++i) {
        sixel_qa_lab(xyz + i * 3, value);
        for (c = 0; c < 3; ++c) {
            lab.values[i * 3 + c] = (float)value[c];
        }
    }
    return lab;
}

static sixel_assessment_float_buffer_t
chroma_ab(const sixel_assessment_float_buffer_t *lab, size_t pixels)
{
    sixel_assessment_float_buffer_t chroma;
    size_t i;
    float a;
    float b;
    chroma = float_buffer_create(pixels);
    for (i = 0; i < pixels; ++i) {
        a = lab->values[i * 3 + 1];
        b = lab->values[i * 3 + 2];
        chroma.values[i] = sqrtf(a * a + b * b);
    }
    return chroma;
}

static sixel_assessment_float_buffer_t
deltaE00(const sixel_assessment_float_buffer_t *lab1,
         const sixel_assessment_float_buffer_t *lab2, size_t pixels)
{
    sixel_assessment_float_buffer_t out;
    size_t i;
    int c;
    double a[3];
    double b[3];

    out = float_buffer_create(pixels);
    for (i = 0; i < pixels; ++i) {
        for (c = 0; c < 3; ++c) {
            a[c] = lab1->values[i * 3 + c];
            b[c] = lab2->values[i * 3 + c];
        }
        out.values[i] = (float)sixel_qa_de00(a, b);
    }
    return out;
}
/*
 * GMSD and PSNR
 */
static float
gmsd_metric(const sixel_assessment_float_buffer_t *ref,
            const sixel_assessment_float_buffer_t *out,
            int width,
            int height)
{
    static const float kx[9] = {0.25f, 0.0f, -0.25f,
                                0.5f, 0.0f, -0.5f,
                                0.25f, 0.0f, -0.25f};
    static const float ky[9] = {0.25f, 0.5f, 0.25f,
                                0.0f, 0.0f, 0.0f,
                                -0.25f, -0.5f, -0.25f};
    sixel_assessment_float_buffer_t gx1;
    sixel_assessment_float_buffer_t gy1;
    sixel_assessment_float_buffer_t gx2;
    sixel_assessment_float_buffer_t gy2;
    sixel_assessment_float_buffer_t gm1;
    sixel_assessment_float_buffer_t gm2;
    double c;
    double mean;
    double sq_sum;
    size_t total;
    int y;
    int x;
    float accx1;
    float accy1;
    float accx2;
    float accy2;
    int dy;
    int dx;
    int yy;
    int xx;
    int kidx;
    size_t idx;
    float mag1;
    float mag2;
    double gms;

    gx1 = float_buffer_create((size_t)width * (size_t)height);
    gy1 = float_buffer_create((size_t)width * (size_t)height);
    gx2 = float_buffer_create((size_t)width * (size_t)height);
    gy2 = float_buffer_create((size_t)width * (size_t)height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            accx1 = 0.0f;
            accy1 = 0.0f;
            accx2 = 0.0f;
            accy2 = 0.0f;
            for (dy = -1; dy <= 1; ++dy) {
                yy = y + dy;
                if (yy < 0) {
                    yy = -yy;
                }
                if (yy >= height) {
                    yy = height - (yy - height) - 1;
                    if (yy < 0) {
                        yy = 0;
                    }
                }
                for (dx = -1; dx <= 1; ++dx) {
                    xx = x + dx;
                    if (xx < 0) {
                        xx = -xx;
                    }
                    if (xx >= width) {
                        xx = width - (xx - width) - 1;
                        if (xx < 0) {
                            xx = 0;
                        }
                    }
                    kidx = (dy + 1) * 3 + (dx + 1);
                    accx1 += ref->values[yy * width + xx] * kx[kidx];
                    accy1 += ref->values[yy * width + xx] * ky[kidx];
                    accx2 += out->values[yy * width + xx] * kx[kidx];
                    accy2 += out->values[yy * width + xx] * ky[kidx];
                }
            }
            gx1.values[y * width + x] = accx1;
            gy1.values[y * width + x] = accy1;
            gx2.values[y * width + x] = accx2;
            gy2.values[y * width + x] = accy2;
        }
    }

    gm1 = float_buffer_create((size_t)width * (size_t)height);
    gm2 = float_buffer_create((size_t)width * (size_t)height);
    total = (size_t)width * (size_t)height;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            idx = (size_t)y * (size_t)width + (size_t)x;
            mag1 = sqrtf(gx1.values[idx] * gx1.values[idx] +
                         gy1.values[idx] * gy1.values[idx]) + 1e-12f;
            mag2 = sqrtf(gx2.values[idx] * gx2.values[idx] +
                         gy2.values[idx] * gy2.values[idx]) + 1e-12f;
            gm1.values[idx] = mag1;
            gm2.values[idx] = mag2;
        }
    }

    c = 0.0026;
    mean = 0.0;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            idx = (size_t)y * (size_t)width + (size_t)x;
            gms = (2.0 * gm1.values[idx] * gm2.values[idx] + c) /
                  (gm1.values[idx] * gm1.values[idx] +
                   gm2.values[idx] * gm2.values[idx] + c);
            mean += gms;
        }
    }
    mean /= (double)total;

    sq_sum = 0.0;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            idx = (size_t)y * (size_t)width + (size_t)x;
            gms = (2.0 * gm1.values[idx] * gm2.values[idx] + c) /
                  (gm1.values[idx] * gm1.values[idx] +
                   gm2.values[idx] * gm2.values[idx] + c);
            sq_sum += (gms - mean) * (gms - mean);
        }
    }
    sq_sum /= (double)total;

    float_buffer_free(&gx1);
    float_buffer_free(&gy1);
    float_buffer_free(&gx2);
    float_buffer_free(&gy2);
    float_buffer_free(&gm1);
    float_buffer_free(&gm2);

    return (float)sqrt(sq_sum);
}

static float
psnr_metric(const sixel_assessment_float_buffer_t *ref,
            const sixel_assessment_float_buffer_t *out,
            int width,
            int height)
{
    double mse;
    size_t total;
    size_t i;
    double diff;

    mse = 0.0;
    total = (size_t)width * (size_t)height;
    for (i = 0; i < total; ++i) {
        diff = ref->values[i] - out->values[i];
        mse += diff * diff;
    }
    mse /= (double)total;
    if (mse <= 1e-12) {
        return 99.0f;
    }
    return (float)(10.0 * log10(1.0 / mse));
}

/*
 * sixel_assessment_metrics_t aggregation
 */
static void
assessment_metric_mark(sixel_assessment_metrics_t *metrics,
                       int metric_id,
                       float value)
{
    if (metrics == NULL) {
        return;
    }
    if (!isfinite((double)value)) {
        return;
    }
    metrics->valid_mask |= SIXEL_ASSESSMENT_METRIC_MASK(metric_id);
}

static void
assessment_metrics_init(sixel_assessment_metrics_t *metrics)
{
    memset(metrics, 0, sizeof(*metrics));
    metrics->valid_mask = 0u;
}

/* Produce physical PCS and a separate, documented luma signal.
 * Never overwrite the source frame or reinterpret opponent channels as RGB.
 * An unchanged gamma/linear signal is copied exactly for MS-SSIM. */
static int
assessment_color_pixels(float *pixels, double *xyz, size_t count,
                         int coordinate, int signal, int bytes,
                         int source_bytes, int grayscale)
{
    sixel_color_def_t *srgb;
    size_t i;
    int c;
    int ok;
    double sample[3];
    double value[3];
    float gray;

    /* Byte samples in the selected basis already have the requested
     * representation. Preserve the established spatial-metric fast path. */
    if (xyz == NULL && source_bytes && coordinate == signal && !grayscale) {
        return 1;
    }
    srgb = xyz != NULL ? sixel_cdef_name("srgb") : NULL;
    if (xyz != NULL && srgb == NULL) {
        return 0;
    }
    ok = 1;
    for (i = 0; i < count; ++i) {
        for (c = 0; c < 3; ++c) {
            sample[c] = pixels[i * 3 + c];
        }
        if (coordinate == signal) {
            /* Identical working coordinates need no TRC round trip. */
            memcpy(value, sample, sizeof(value));
        }
        if (!isfinite(sample[0]) || !isfinite(sample[1])
                || !isfinite(sample[2])
                || (xyz != NULL
                    && !sixel_qa_xyz(srgb, coordinate, sample, xyz + i * 3))
                || (coordinate != signal
                    && !sixel_color_convert(value, sample,
                                             coordinate, signal))) {
            ok = 0;
            break;
        }
        for (c = 0; c < 3; ++c) {
            if (bytes && (!source_bytes || coordinate != signal)) {
                value[c] = floor(fmin(1.0, fmax(0.0, value[c]))
                                   * 255.0 + 0.5) / 255.0;
            }
            pixels[i * 3 + c] = (float)value[c];
            if (!isfinite(pixels[i * 3 + c])) {
                ok = 0;
                break;
            }
        }
        if (!ok) {
            break;
        }
        if (grayscale) {
            gray = 0.2126f * pixels[i * 3]
                + 0.7152f * pixels[i * 3 + 1]
                + 0.0722f * pixels[i * 3 + 2];
            if (bytes) {
                gray = floorf(gray * 255.0f + 0.5f) / 255.0f;
            }
            for (c = 0; c < 3; ++c) {
                pixels[i * 3 + c] = gray;
            }
        }
    }
    sixel_cdef_free(srgb);
    return ok;
}

static sixel_assessment_metrics_t
evaluate_metrics(sixel_assessment_t *assessment,
                 const float *ref_pixels,
                 const float *out_pixels,
                 const double *ref_xyz,
                 const double *out_xyz,
                 int width,
                 int height)
{
    sixel_assessment_metrics_t metrics;
    sixel_assessment_float_buffer_t ref_luma;
    sixel_assessment_float_buffer_t out_luma;
    sixel_assessment_float_buffer_t ref_lab;
    sixel_assessment_float_buffer_t out_lab;
    sixel_assessment_float_buffer_t ref_chroma;
    sixel_assessment_float_buffer_t out_chroma;
    sixel_assessment_float_buffer_t de00;
    size_t pixels;
    size_t iter;
    double sum_value;
    double maximum;
    static double const thresholds[5] = {1, 5, 10, 20, 50};
    size_t peak;
    int threshold;

    int want_ms_ssim;
    int want_vif_p;
    int want_high_freq_out;
    int want_high_freq_ref;
    int want_high_freq_delta;
    int want_stripe_out;
    int want_stripe_ref;
    int want_stripe_rel;
    int want_banding_run_rel;
    int want_banding_grad_rel;
    int want_clip_out;
    int want_clip_ref;
    int want_clip_rel;
    int want_delta_chroma_mean;
    int want_delta_e00_mean;
    int want_delta_e00_max;
    int want_gmsd;
    int want_psnr_y;
    int need_luma;
    int need_lab;
    int need_clip;
    float high_freq_out_value;
    float high_freq_ref_value;
    float stripe_ref_value;
    float stripe_out_value;
    float band_run_out;
    float band_run_ref;
    float band_grad_out;
    float band_grad_ref;

    assessment_metrics_init(&metrics);

    high_freq_out_value = 0.0f;
    high_freq_ref_value = 0.0f;
    stripe_ref_value = 0.0f;
    stripe_out_value = 0.0f;
    band_run_out = 0.0f;
    band_run_ref = 0.0f;
    band_grad_out = 0.0f;
    band_grad_ref = 0.0f;

    ref_luma.length = 0;
    ref_luma.values = NULL;
    out_luma = ref_luma;
    ref_lab = ref_luma;
    out_lab = ref_luma;
    ref_chroma = ref_luma;
    out_chroma = ref_luma;
    de00 = ref_luma;

    want_ms_ssim = assessment_metric_selected(assessment,
                   SIXEL_ASSESSMENT_METRIC_MS_SSIM);
    want_vif_p = assessment_metric_selected(assessment,
                  SIXEL_ASSESSMENT_METRIC_VIF_P);
    want_high_freq_delta = assessment_metric_selected(assessment,
                           SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_DELTA);
    want_high_freq_out = assessment_metric_selected(assessment,
                        SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_OUT) ||
                        want_high_freq_delta;
    want_high_freq_ref = assessment_metric_selected(assessment,
                        SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_REF) ||
                        want_high_freq_delta;
    want_stripe_rel = assessment_metric_selected(assessment,
                     SIXEL_ASSESSMENT_METRIC_STRIPE_REL);
    want_stripe_out = assessment_metric_selected(assessment,
                     SIXEL_ASSESSMENT_METRIC_STRIPE_OUT) ||
                     want_stripe_rel;
    want_stripe_ref = assessment_metric_selected(assessment,
                     SIXEL_ASSESSMENT_METRIC_STRIPE_REF) ||
                     want_stripe_rel;
    want_banding_run_rel = assessment_metric_selected(assessment,
                          SIXEL_ASSESSMENT_METRIC_BAND_RUN_REL);
    want_banding_grad_rel = assessment_metric_selected(assessment,
                           SIXEL_ASSESSMENT_METRIC_BAND_GRAD_REL);
    want_clip_ref = assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_L_REF) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_R_REF) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_G_REF) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_B_REF) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_L_REL) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_R_REL) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_G_REL) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_B_REL);
    want_clip_out = assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_L_OUT) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_R_OUT) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_G_OUT) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_B_OUT) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_L_REL) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_R_REL) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_G_REL) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_B_REL);
    want_clip_rel = assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_L_REL) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_R_REL) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_G_REL) ||
                    assessment_metric_selected(assessment,
                    SIXEL_ASSESSMENT_METRIC_CLIP_B_REL);
    want_delta_chroma_mean = assessment_metric_selected(assessment,
                              SIXEL_ASSESSMENT_METRIC_DELTA_CHROMA);
    want_delta_e00_mean = assessment_metric_selected(assessment,
                           SIXEL_ASSESSMENT_METRIC_DELTA_E00);
    want_delta_e00_max = assessment_metric_selected(assessment,
                           SIXEL_ASSESSMENT_METRIC_DELTA_E00_MAX);
    want_gmsd = assessment_metric_selected(assessment,
                 SIXEL_ASSESSMENT_METRIC_GMSD);
    want_psnr_y = assessment_metric_selected(assessment,
                   SIXEL_ASSESSMENT_METRIC_PSNR_Y);
    need_luma = want_ms_ssim || want_vif_p ||
                want_high_freq_out || want_high_freq_ref ||
                want_stripe_out || want_stripe_ref || want_banding_run_rel ||
                want_banding_grad_rel || want_gmsd || want_psnr_y;
    need_lab = want_delta_chroma_mean || want_delta_e00_mean ||
        want_delta_e00_max;
    need_clip = want_clip_ref || want_clip_out;

    if (need_luma != 0) {
        ref_luma = pixels_to_luma709(ref_pixels, width, height);
        out_luma = pixels_to_luma709(out_pixels, width, height);
    }
    if (want_ms_ssim) {
        metrics.ms_ssim = ms_ssim_luma(&ref_luma, &out_luma,
                                      width, height);
        assessment_metric_mark(&metrics,
            SIXEL_ASSESSMENT_METRIC_MS_SSIM, metrics.ms_ssim);
    }
    if (want_vif_p) {
        metrics.vif_p = vif_p_luma(&ref_luma, &out_luma, width, height);
        assessment_metric_mark(&metrics,
            SIXEL_ASSESSMENT_METRIC_VIF_P, metrics.vif_p);
    }
    if (need_luma && (want_high_freq_out || want_high_freq_delta)) {
        high_freq_out_value = high_frequency_ratio(&out_luma,
                                                   width,
                                                   height,
                                                   0.25f);
    }
    if (need_luma && (want_high_freq_ref || want_high_freq_delta)) {
        high_freq_ref_value = high_frequency_ratio(&ref_luma,
                                                   width,
                                                   height,
                                                   0.25f);
    }
    if (want_high_freq_out && need_luma &&
            assessment_metric_selected(assessment,
                SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_OUT)) {
        metrics.high_freq_out = high_freq_out_value;
        assessment_metric_mark(&metrics,
                               SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_OUT,
                               metrics.high_freq_out);
    }
    if (want_high_freq_ref && need_luma &&
            assessment_metric_selected(assessment,
                SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_REF)) {
        metrics.high_freq_ref = high_freq_ref_value;
        assessment_metric_mark(&metrics,
                               SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_REF,
                               metrics.high_freq_ref);
    }
    if (want_high_freq_delta && need_luma) {
        metrics.high_freq_delta = high_freq_out_value - high_freq_ref_value;
        assessment_metric_mark(&metrics,
                               SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_DELTA,
                               metrics.high_freq_delta);
    }
    if (need_luma && (want_stripe_ref || want_stripe_rel)) {
        stripe_ref_value = stripe_score(&ref_luma, width, height, 180);
    }
    if (need_luma && (want_stripe_out || want_stripe_rel)) {
        stripe_out_value = stripe_score(&out_luma, width, height, 180);
    }
    if (want_stripe_ref && need_luma &&
            assessment_metric_selected(assessment,
                SIXEL_ASSESSMENT_METRIC_STRIPE_REF)) {
        metrics.stripe_ref = stripe_ref_value;
        assessment_metric_mark(&metrics,
                               SIXEL_ASSESSMENT_METRIC_STRIPE_REF,
                               metrics.stripe_ref);
    }
    if (want_stripe_out && need_luma &&
            assessment_metric_selected(assessment,
                SIXEL_ASSESSMENT_METRIC_STRIPE_OUT)) {
        metrics.stripe_out = stripe_out_value;
        assessment_metric_mark(&metrics,
                               SIXEL_ASSESSMENT_METRIC_STRIPE_OUT,
                               metrics.stripe_out);
    }
    if (want_stripe_rel && need_luma) {
        metrics.stripe_rel = stripe_out_value - stripe_ref_value;
        assessment_metric_mark(&metrics,
                               SIXEL_ASSESSMENT_METRIC_STRIPE_REL,
                               metrics.stripe_rel);
    }
    if (want_banding_run_rel && need_luma) {
        band_run_out = banding_index_runlen(&out_luma, width, height, 32);
        band_run_ref = banding_index_runlen(&ref_luma, width, height, 32);
        metrics.band_run_rel = band_run_out - band_run_ref;
        assessment_metric_mark(&metrics,
                               SIXEL_ASSESSMENT_METRIC_BAND_RUN_REL,
                               metrics.band_run_rel);
    }
    if (want_banding_grad_rel && need_luma) {
        band_grad_out = banding_index_gradient(&out_luma, width, height);
        band_grad_ref = banding_index_gradient(&ref_luma, width, height);
        metrics.band_grad_rel = band_grad_out - band_grad_ref;
        assessment_metric_mark(&metrics,
                               SIXEL_ASSESSMENT_METRIC_BAND_GRAD_REL,
                               metrics.band_grad_rel);
    }
    if (need_clip != 0) {
        if (want_clip_ref) {
            clipping_rates(ref_pixels, width, height,
                           &metrics.clip_l_ref,
                           &metrics.clip_r_ref,
                           &metrics.clip_g_ref,
                           &metrics.clip_b_ref);
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_CLIP_L_REF,
                                   metrics.clip_l_ref);
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_CLIP_R_REF,
                                   metrics.clip_r_ref);
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_CLIP_G_REF,
                                   metrics.clip_g_ref);
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_CLIP_B_REF,
                                   metrics.clip_b_ref);
        }
        if (want_clip_out) {
            clipping_rates(out_pixels, width, height,
                           &metrics.clip_l_out,
                           &metrics.clip_r_out,
                           &metrics.clip_g_out,
                           &metrics.clip_b_out);
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_CLIP_L_OUT,
                                   metrics.clip_l_out);
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_CLIP_R_OUT,
                                   metrics.clip_r_out);
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_CLIP_G_OUT,
                                   metrics.clip_g_out);
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_CLIP_B_OUT,
                                   metrics.clip_b_out);
        }
        if (want_clip_rel) {
            metrics.clip_l_rel = metrics.clip_l_out - metrics.clip_l_ref;
            metrics.clip_r_rel = metrics.clip_r_out - metrics.clip_r_ref;
            metrics.clip_g_rel = metrics.clip_g_out - metrics.clip_g_ref;
            metrics.clip_b_rel = metrics.clip_b_out - metrics.clip_b_ref;
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_CLIP_L_REL,
                                   metrics.clip_l_rel);
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_CLIP_R_REL,
                                   metrics.clip_r_rel);
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_CLIP_G_REL,
                                   metrics.clip_g_rel);
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_CLIP_B_REL,
                                   metrics.clip_b_rel);
        }
    }
    if (need_lab != 0) {
        ref_lab = pcs_to_lab(ref_xyz, width, height);
        out_lab = pcs_to_lab(out_xyz, width, height);
        pixels = (size_t)width * (size_t)height;
        if (want_delta_chroma_mean) {
            ref_chroma = chroma_ab(&ref_lab, pixels);
            out_chroma = chroma_ab(&out_lab, pixels);
            sum_value = 0.0;
            for (iter = 0; iter < pixels; ++iter) {
                sum_value += fabs(out_chroma.values[iter] -
                                  ref_chroma.values[iter]);
            }
            metrics.delta_chroma_mean =
                (float)(sum_value / (double)pixels);
            assessment_metric_mark(&metrics,
                                   SIXEL_ASSESSMENT_METRIC_DELTA_CHROMA,
                                   metrics.delta_chroma_mean);
        }
        if (want_delta_e00_mean || want_delta_e00_max) {
            de00 = deltaE00(&ref_lab, &out_lab, pixels);
            sum_value = 0.0;
            maximum = 0.0;
            peak = 0;
            memset(assessment->delta_e00_above, 0,
                   sizeof(assessment->delta_e00_above));
            for (iter = 0; iter < pixels; ++iter) {
                sum_value += de00.values[iter];
                if (want_delta_e00_max) {
                    /* An undefined sample must not become a valid zero
                     * maximum just because comparisons with NaN are false.
                     * metric_mark omits nonfinite results and diagnostics. */
                    if (!isfinite((double)de00.values[iter])) {
                        maximum = NAN;
                    }
                    if (de00.values[iter] > maximum) {
                        maximum = de00.values[iter];
                        peak = iter;
                    }
                    for (threshold = 0; threshold < 5; ++threshold) {
                        if (de00.values[iter] > thresholds[threshold]) {
                            ++assessment->delta_e00_above[threshold];
                        }
                    }
                }
            }
            if (want_delta_e00_mean) {
                metrics.delta_e00_mean = (float)(sum_value / (double)pixels);
                assessment_metric_mark(&metrics,
                                       SIXEL_ASSESSMENT_METRIC_DELTA_E00,
                                       metrics.delta_e00_mean);
            }
            if (want_delta_e00_max) {
                metrics.delta_e00_max = (float)maximum;
                assessment->delta_e00_max_x = (int)(peak % (size_t)width);
                assessment->delta_e00_max_y = (int)(peak / (size_t)width);
                assessment_metric_mark(&metrics,
                                       SIXEL_ASSESSMENT_METRIC_DELTA_E00_MAX,
                                       metrics.delta_e00_max);
            }
        }
    }
    if (want_gmsd && need_luma) {
        metrics.gmsd_value = gmsd_metric(&ref_luma, &out_luma,
                                         width, height);
        assessment_metric_mark(&metrics,
                               SIXEL_ASSESSMENT_METRIC_GMSD,
                               metrics.gmsd_value);
    }
    if (want_psnr_y && need_luma) {
        metrics.psnr_y = psnr_metric(&ref_luma, &out_luma, width, height);
        assessment_metric_mark(&metrics,
                               SIXEL_ASSESSMENT_METRIC_PSNR_Y,
                               metrics.psnr_y);
    }

    if (ref_luma.values != NULL) {
        float_buffer_free(&ref_luma);
    }
    if (out_luma.values != NULL) {
        float_buffer_free(&out_luma);
    }
    if (ref_lab.values != NULL) {
        float_buffer_free(&ref_lab);
    }
    if (out_lab.values != NULL) {
        float_buffer_free(&out_lab);
    }
    if (ref_chroma.values != NULL) {
        float_buffer_free(&ref_chroma);
    }
    if (out_chroma.values != NULL) {
        float_buffer_free(&out_chroma);
    }
    if (de00.values != NULL) {
        float_buffer_free(&de00);
    }

    return metrics;
}

static void
align_frame_pixels(float **ref_pixels,
                   int *ref_width,
                   int *ref_height,
                   float **out_pixels,
                   int *out_width,
                   int *out_height)
{
    int width;
    int height;
    int channels;
    float *ref_new;
    float *out_new;
    int y;
    size_t row_bytes;

    if (ref_pixels == NULL || ref_width == NULL || ref_height == NULL ||
            out_pixels == NULL || out_width == NULL || out_height == NULL) {
        assessment_fail(SIXEL_BAD_ARGUMENT,
                       "align_frame_pixels: invalid parameters");
        return;
    }

    channels = SIXEL_ASSESSMENT_RGB_CHANNELS;
    width = *ref_width < *out_width ? *ref_width : *out_width;
    height = *ref_height < *out_height ? *ref_height : *out_height;
    if (width <= 0 || height <= 0) {
        assessment_fail(SIXEL_BAD_ARGUMENT,
                       "align_frame_pixels: empty frame dimensions");
        return;
    }
    ref_new = (float *)xmalloc((size_t)width * (size_t)height *
                               (size_t)channels * sizeof(float));
    out_new = (float *)xmalloc((size_t)width * (size_t)height *
                               (size_t)channels * sizeof(float));
    row_bytes = (size_t)width * (size_t)channels * sizeof(float);
    for (y = 0; y < height; ++y) {
        memcpy(ref_new + (size_t)y * (size_t)width * (size_t)channels,
               *ref_pixels + (size_t)y * (size_t)(*ref_width) *
               (size_t)channels,
               row_bytes);
        memcpy(out_new + (size_t)y * (size_t)width * (size_t)channels,
               *out_pixels + (size_t)y * (size_t)(*out_width) *
               (size_t)channels,
               row_bytes);
    }
    free(*ref_pixels);
    free(*out_pixels);
    *ref_pixels = ref_new;
    *out_pixels = out_new;
    *ref_width = width;
    *ref_height = height;
    *out_width = width;
    *out_height = height;
}

/*
 * Assessment API bridge
 */
typedef struct MetricDescriptor {
    int id;
    const char *json_key;
} MetricDescriptor;

static const MetricDescriptor g_metric_table[] = {
    {SIXEL_ASSESSMENT_METRIC_SCIELAB, "S-CIELAB"},
    {SIXEL_ASSESSMENT_METRIC_SCIELAB_MAX, "S-CIELAB_max"},
    {SIXEL_ASSESSMENT_METRIC_MS_SSIM, "MS-SSIM"},
    {SIXEL_ASSESSMENT_METRIC_VIF_P, "VIF-P"},
    {SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_OUT, "HighFreqRatio_out"},
    {SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_REF, "HighFreqRatio_ref"},
    {SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_DELTA, "HighFreqRatio_delta"},
    {SIXEL_ASSESSMENT_METRIC_STRIPE_REF, "StripeScore_ref"},
    {SIXEL_ASSESSMENT_METRIC_STRIPE_OUT, "StripeScore_out"},
    {SIXEL_ASSESSMENT_METRIC_STRIPE_REL, "StripeScore_rel"},
    {SIXEL_ASSESSMENT_METRIC_BAND_RUN_REL, "BandingIndex_rel"},
    {SIXEL_ASSESSMENT_METRIC_BAND_GRAD_REL, "BandingIndex_grad_rel"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_L_REF, "ClipRate_L_ref"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_R_REF, "ClipRate_R_ref"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_G_REF, "ClipRate_G_ref"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_B_REF, "ClipRate_B_ref"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_L_OUT, "ClipRate_L_out"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_R_OUT, "ClipRate_R_out"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_G_OUT, "ClipRate_G_out"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_B_OUT, "ClipRate_B_out"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_L_REL, "ClipRate_L_rel"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_R_REL, "ClipRate_R_rel"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_G_REL, "ClipRate_G_rel"},
    {SIXEL_ASSESSMENT_METRIC_CLIP_B_REL, "ClipRate_B_rel"},
    {SIXEL_ASSESSMENT_METRIC_DELTA_CHROMA, "Δ Chroma_mean"},
    {SIXEL_ASSESSMENT_METRIC_DELTA_E00, "Δ E00_mean"},
    {SIXEL_ASSESSMENT_METRIC_DELTA_E00_MAX, "DE00_max"},
    {SIXEL_ASSESSMENT_METRIC_GMSD, "GMSD"},
    {SIXEL_ASSESSMENT_METRIC_PSNR_Y, "PSNR_Y"},
    {SIXEL_ASSESSMENT_METRIC_REGION_MEAN, "DE00_region_mean"},
    {SIXEL_ASSESSMENT_METRIC_REGION_MAX, "DE00_region_max"},
};

static void
store_metric_if_valid(double *results,
                      unsigned int mask,
                      int metric_id,
                      double value)
{
    if ((mask & SIXEL_ASSESSMENT_METRIC_MASK(metric_id)) == 0u) {
        return;
    }
    results[SIXEL_ASSESSMENT_INDEX(metric_id)] = value;
}

static void
store_metrics(sixel_assessment_t *assessment,
              const sixel_assessment_metrics_t *metrics)
{
    double *results;
    unsigned int mask;

    if (assessment == NULL || metrics == NULL) {
        return;
    }
    results = assessment->results;
    memset(results, 0, sizeof(assessment->results));
    mask = metrics->valid_mask;
    assessment->results_valid_mask = mask;

    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_MS_SSIM,
                          metrics->ms_ssim);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_VIF_P,
                          metrics->vif_p);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_OUT,
                          metrics->high_freq_out);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_REF,
                          metrics->high_freq_ref);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_DELTA,
                          metrics->high_freq_delta);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_STRIPE_REF,
                          metrics->stripe_ref);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_STRIPE_OUT,
                          metrics->stripe_out);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_STRIPE_REL,
                          metrics->stripe_rel);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_BAND_RUN_REL,
                          metrics->band_run_rel);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_BAND_GRAD_REL,
                          metrics->band_grad_rel);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_CLIP_L_REF,
                          metrics->clip_l_ref);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_CLIP_R_REF,
                          metrics->clip_r_ref);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_CLIP_G_REF,
                          metrics->clip_g_ref);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_CLIP_B_REF,
                          metrics->clip_b_ref);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_CLIP_L_OUT,
                          metrics->clip_l_out);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_CLIP_R_OUT,
                          metrics->clip_r_out);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_CLIP_G_OUT,
                          metrics->clip_g_out);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_CLIP_B_OUT,
                          metrics->clip_b_out);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_CLIP_L_REL,
                          metrics->clip_l_rel);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_CLIP_R_REL,
                          metrics->clip_r_rel);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_CLIP_G_REL,
                          metrics->clip_g_rel);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_CLIP_B_REL,
                          metrics->clip_b_rel);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_DELTA_CHROMA,
                          metrics->delta_chroma_mean);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_DELTA_E00,
                          metrics->delta_e00_mean);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_DELTA_E00_MAX,
                          metrics->delta_e00_max);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_GMSD,
                          metrics->gmsd_value);
    store_metric_if_valid(results,
                          mask,
                          SIXEL_ASSESSMENT_METRIC_PSNR_Y,
                          metrics->psnr_y);
}

static SIXELSTATUS
sixel_assessment_capture_first_frame(sixel_frame_t *frame,
                                     void *user_data)
{
    sixel_assessment_capture_t *capture;

    /*
     * Loader pipeline sketch for encoded round trips:
     *
     *     +--------------+     +-----------------+
     *     | decoder loop | --> | capture.frame   |
     *     +--------------+     +-----------------+
     */

    capture = (sixel_assessment_capture_t *)user_data;
    if (capture == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (capture->frame == NULL) {
        sixel_frame_ref(frame);
        capture->frame = frame;
    }
    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_assessment_load_single_frame(char const *path,
                                   sixel_allocator_t *allocator,
                                   sixel_frame_t **ppframe)
{
    SIXELSTATUS status;
    sixel_loader_t *loader;
    sixel_assessment_capture_t capture;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_override;
    int finsecure;

    /*
     * Reload a single-frame image with the regular loader stack.  This helper
     * is used when the encoded SIXEL output needs to be analyzed by the
     * assessment pipeline.
     */

    status = SIXEL_FALSE;
    loader = NULL;
    capture.frame = NULL;
    fstatic = 1;
    fuse_palette = 0;
    reqcolors = SIXEL_PALETTE_MAX;
    loop_override = SIXEL_LOOP_DISABLE;
    finsecure = 0;

    if (path == NULL || allocator == NULL || ppframe == NULL) {
        sixel_helper_set_additional_message(
            "sixel_assessment_load_single_frame: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                 &fstatic);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_USE_PALETTE,
                                 &fuse_palette);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQCOLORS,
                                 &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                 &loop_override);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_INSECURE,
                                 &finsecure);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 &capture);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_load_file(loader,
                                    path,
                                    sixel_assessment_capture_first_frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (capture.frame == NULL) {
        sixel_helper_set_additional_message(
            "sixel_assessment_load_single_frame: no frames captured.");
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    sixel_frame_ref(capture.frame);
    *ppframe = capture.frame;
    capture.frame = NULL;
    status = SIXEL_OK;

cleanup:
    if (capture.frame != NULL) {
        sixel_frame_unref(capture.frame);
    }
    if (loader != NULL) {
        sixel_loader_unref(loader);
    }
    return status;
}

SIXELSTATUS
sixel_assessment_new(sixel_assessment_t **ppassessment,
                    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_assessment_t *assessment;

    if (ppassessment == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    assessment = (sixel_assessment_t *)sixel_allocator_malloc(
        allocator,
        sizeof(sixel_assessment_t));
    if (assessment == NULL) {
        sixel_allocator_unref(allocator);
        return SIXEL_BAD_ALLOCATION;
    }

    assessment->refcount = 1U;
    assessment->allocator = allocator;
    assessment->results_ready = 0;
    assessment->last_error = SIXEL_OK;
    assessment->error_message[0] = '\0';
    assessment->results_valid_mask = 0u;
    memset(assessment->results, 0,
           sizeof(assessment->results));
    assessment_reset_stage_bookkeeping(assessment);
    assessment->input_path[0] = '\0';
    assessment->loader_name[0] = '\0';
    assessment->format_name[0] = '\0';
    assessment->input_pixelformat = (-1);
    assessment->input_colorspace = (-1);
    assessment->input_bytes = 0u;
    assessment->source_pixels_bytes = 0u;
    assessment->quantized_pixels_bytes = 0u;
    assessment->palette_bytes = 0u;
    assessment->palette_colors = 0;
    assessment->output_bytes = 0u;
    assessment->output_bytes_written = 0u;
    assessment->metrics_mask = assessment_metric_mask_all();
    assessment->sections_mask = SIXEL_ASSESSMENT_SECTION_NONE;
    assessment->view_mask = SIXEL_ASSESSMENT_VIEW_ENCODED;
    assessment->signal_space = -1;
    assessment->signal_bytes = 0;
    assessment->grayscale = 0;
    assessment->scielab_ppd = SIXEL_QA_SCIELAB_PPD_DEFAULT;
    assessment->scielab_map = NULL;
    assessment->scielab_map_capacity = 0U;
    assessment->region_window = 16;
    assessment->region_count = 0;
    memset(assessment->current_space, 0, sizeof(assessment->current_space));
    memset(assessment->current_format, 0, sizeof(assessment->current_format));
    memset(assessment->background, 0, sizeof(assessment->background));
    assessment->color_spec[0] = "auto";
    assessment->color_spec[1] = "auto";
    assessment->color_origin[0] = "frame-working-srgb";
    assessment->color_origin[1] = "frame-working-srgb";
    assessment->direct = 0;
    assessment->drawing[0] = "not-selected";
    assessment->drawing[1] = "not-selected";

    *ppassessment = assessment;
    return SIXEL_OK;
}

void
sixel_assessment_ref(sixel_assessment_t *assessment)
{
    if (assessment == NULL) {
        return;
    }
    (void)sixel_atomic_fetch_add_u32(&assessment->refcount, 1U);
}

void
sixel_assessment_unref(sixel_assessment_t *assessment)
{
    unsigned int previous;

    if (assessment == NULL) {
        return;
    }
    previous = sixel_atomic_fetch_sub_u32(&assessment->refcount, 1U);
    if (previous == 1U) {
        sixel_allocator_t *allocator;

        allocator = assessment->allocator;
        sixel_allocator_free(allocator, assessment);
        sixel_allocator_unref(allocator);
    }
}

SIXELSTATUS
sixel_assessment_setopt(sixel_assessment_t *assessment,
                       int option,
                       char const *value)
{
    if (assessment == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    (void)option;
    (void)value;
    return SIXEL_BAD_ARGUMENT;
}

SIXELSTATUS
sixel_assessment_analyze(sixel_assessment_t *assessment,
                         sixel_frame_t *reference,
                         sixel_frame_t *output)
{
    struct qa_owned_buffers {
        float *ref_pixels;
        float *out_pixels;
        double *ref_xyz;
        double *out_xyz;
    } *buffers;
    sixel_assessment_metrics_t metrics;
    SIXELSTATUS status;
    int bail;
    int ref_width;
    int ref_height;
    int out_width;
    int out_height;
    double region_mean;
    double region_max;
    size_t pixels;
    int signal;
    int need_xyz;
    int need_regions;
    int need_scielab;
    double scielab_mean;
    double scielab_max;

    if (assessment == NULL || reference == NULL || output == NULL
            || (assessment->region_window != 8
                && assessment->region_window != 16
                && assessment->region_window != 32
                && assessment->region_window != 64)) {
        return SIXEL_BAD_ARGUMENT;
    }

    /* The existing metric allocator reports failure with longjmp.
     * Keep buffer ownership in heap storage so cleanup remains defined. */
    buffers = (struct qa_owned_buffers *)calloc(1, sizeof(*buffers));
    if (buffers == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    region_mean = 0.0;
    region_max = 0.0;
    buffers->ref_pixels = NULL;
    buffers->out_pixels = NULL;
    ref_width = 0;
    ref_height = 0;
    out_width = 0;
    out_height = 0;
    buffers->ref_xyz = NULL;
    buffers->out_xyz = NULL;
    signal = assessment->signal_space;
    if (signal < 0) {
        signal = sixel_frame_get_colorspace(reference);
    }
    /* Opponent comparison choices use physical linear-sRGB Y. */
    if (signal != SIXEL_COLORSPACE_GAMMA) {
        signal = SIXEL_COLORSPACE_LINEAR;
    }
    /* Use the stored choice below the bailout checkpoint so the temporary
     * normalization variable does not remain live across setjmp. */
    assessment->signal_space = signal;
    assessment->current_space[0] = sixel_frame_get_colorspace(reference);
    assessment->current_space[1] = sixel_frame_get_colorspace(output);
    assessment->current_format[0] = sixel_frame_get_pixelformat(reference);
    assessment->current_format[1] = sixel_frame_get_pixelformat(output);

    assessment->last_error = SIXEL_OK;
    assessment->error_message[0] = '\0';
    assessment->results_ready = 0;
    assessment->results_valid_mask = 0u;
    memset(assessment->results, 0,
           sizeof(assessment->results));
    g_assessment_context = assessment;
    bail = setjmp(assessment->bailout);
    if (bail != 0) {
        status = assessment->last_error;
        goto cleanup;
    }

    status = sixel_frame_strip_alpha(reference, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_strip_alpha(output, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = frame_to_rgb_float(reference,
                                &buffers->ref_pixels,
                                &ref_width,
                                &ref_height);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = frame_to_rgb_float(output,
                                &buffers->out_pixels,
                                &out_width,
                                &out_height);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    align_frame_pixels(&buffers->ref_pixels,
                       &ref_width,
                       &ref_height,
                       &buffers->out_pixels,
                       &out_width,
                       &out_height);

    pixels = (size_t)ref_width * (size_t)ref_height;
    if (pixels > SIZE_MAX / (3 * sizeof(double))) {
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    need_regions = assessment_metric_selected(assessment,
        SIXEL_ASSESSMENT_METRIC_REGION_MEAN)
        || assessment_metric_selected(assessment,
        SIXEL_ASSESSMENT_METRIC_REGION_MAX);
    need_scielab = assessment_metric_selected(assessment,
        SIXEL_ASSESSMENT_METRIC_SCIELAB)
        || assessment_metric_selected(assessment,
        SIXEL_ASSESSMENT_METRIC_SCIELAB_MAX)
        || assessment->scielab_map != NULL;
    if (assessment->scielab_map != NULL
            && assessment->scielab_map_capacity < pixels) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    need_xyz = need_regions || need_scielab
        || assessment_metric_selected(assessment,
        SIXEL_ASSESSMENT_METRIC_DELTA_E00)
        || assessment_metric_selected(assessment,
        SIXEL_ASSESSMENT_METRIC_DELTA_E00_MAX)
        || assessment_metric_selected(assessment,
        SIXEL_ASSESSMENT_METRIC_DELTA_CHROMA);
    if (need_xyz) {
        buffers->ref_xyz = (double *)malloc(pixels * 3 * sizeof(double));
        buffers->out_xyz = (double *)malloc(pixels * 3 * sizeof(double));
        if (buffers->ref_xyz == NULL || buffers->out_xyz == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
    }
    if (!assessment_color_pixels(buffers->ref_pixels, buffers->ref_xyz, pixels,
            assessment->current_space[0], assessment->signal_space,
            assessment->signal_bytes,
            !SIXEL_PIXELFORMAT_IS_FLOAT32(assessment->current_format[0]),
            assessment->grayscale)
            || !assessment_color_pixels(buffers->out_pixels,
            buffers->out_xyz, pixels,
            assessment->current_space[1], assessment->signal_space,
            assessment->signal_bytes,
            !SIXEL_PIXELFORMAT_IS_FLOAT32(assessment->current_format[1]),
            assessment->grayscale)) {
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    metrics = evaluate_metrics(assessment,
                               buffers->ref_pixels,
                               buffers->out_pixels,
                               buffers->ref_xyz,
                               buffers->out_xyz,
                               ref_width,
                               ref_height);
    store_metrics(assessment, &metrics);
    assessment->region_count =
        ((size_t)ref_width + assessment->region_window - 1)
        / assessment->region_window
        * (((size_t)ref_height + assessment->region_window - 1)
           / assessment->region_window);
    if (need_regions) {
        sixel_qa_regions(buffers->ref_xyz, buffers->out_xyz,
                          ref_width, ref_height,
                          assessment->region_window, &region_mean, &region_max,
                          &assessment->region_count);
    }
    if (assessment_metric_selected(assessment,
                                  SIXEL_ASSESSMENT_METRIC_REGION_MEAN)) {
        assessment->results[25] = region_mean;
        assessment->results_valid_mask |= 1u << 25;
    }
    if (assessment_metric_selected(assessment,
                                  SIXEL_ASSESSMENT_METRIC_REGION_MAX)) {
        assessment->results[26] = region_max;
        assessment->results_valid_mask |= 1u << 26;
    }
    if (need_scielab) {
        status = sixel_qa_scielab(buffers->ref_xyz, buffers->out_xyz,
            ref_width, ref_height, assessment->scielab_ppd,
            &scielab_mean, &scielab_max, assessment->scielab_map);
        if (SIXEL_FAILED(status)) {
            assessment->results_valid_mask = 0u;
            goto cleanup;
        }
        if (assessment_metric_selected(assessment,
                SIXEL_ASSESSMENT_METRIC_SCIELAB)) {
            assessment->results[SIXEL_ASSESSMENT_INDEX(
                SIXEL_ASSESSMENT_METRIC_SCIELAB)] = scielab_mean;
            assessment->results_valid_mask |= SIXEL_ASSESSMENT_METRIC_MASK(
                SIXEL_ASSESSMENT_METRIC_SCIELAB);
        }
        if (assessment_metric_selected(assessment,
                SIXEL_ASSESSMENT_METRIC_SCIELAB_MAX)) {
            assessment->results[SIXEL_ASSESSMENT_INDEX(
                SIXEL_ASSESSMENT_METRIC_SCIELAB_MAX)] = scielab_max;
            assessment->results_valid_mask |= SIXEL_ASSESSMENT_METRIC_MASK(
                SIXEL_ASSESSMENT_METRIC_SCIELAB_MAX);
        }
    }
    assessment->results_ready = 1;
    status = SIXEL_OK;

cleanup:
    free(buffers->ref_xyz);
    free(buffers->out_xyz);
    if (buffers->ref_pixels != NULL) {
        free(buffers->ref_pixels);
    }
    if (buffers->out_pixels != NULL) {
        free(buffers->out_pixels);
    }
    free(buffers);
    g_assessment_context = NULL;
    return status;
}

SIXEL_INTERNAL_API char const *
sixel_assessment_metric_name(int index)
{
    size_t i;

    for (i = 0; i < sizeof(g_metric_table) / sizeof(g_metric_table[0]); ++i) {
        if (SIXEL_ASSESSMENT_INDEX(g_metric_table[i].id) == index) {
            return g_metric_table[i].json_key;
        }
    }
    return NULL;
}

static SIXELSTATUS
assessment_emit_quality_lines(sixel_assessment_t *assessment,
                              sixel_assessment_json_callback_t callback,
                              void *user_data,
                              char const *indent)
{
    enum { line_buffer_size = SIXEL_ASSESSMENT_PATH_CAPACITY * 2 + 128 };
    size_t i;
    char line[line_buffer_size];
    int written;
    int last;
    int index;
    double value;

    if (indent == NULL) {
        indent = "";
    }
    for (i = 0; i < sizeof(g_metric_table) / sizeof(g_metric_table[0]); ++i) {
        last = (i + 1 == sizeof(g_metric_table) /
                sizeof(g_metric_table[0]));
        index = SIXEL_ASSESSMENT_INDEX(g_metric_table[i].id);
        if (index < 0 || index >= SIXEL_ASSESSMENT_METRIC_COUNT) {
            written = snprintf(line,
                               sizeof(line),
                               "%s\"%s\": null%s\n",
                               indent,
                               g_metric_table[i].json_key,
                               last ? "" : ",");
            if (written < 0 || (size_t)written >= sizeof(line)) {
                return SIXEL_RUNTIME_ERROR;
            }
            callback(line, (size_t)written, user_data);
            continue;
        }
        if ((assessment->results_valid_mask &
                SIXEL_ASSESSMENT_METRIC_MASK(g_metric_table[i].id)) == 0u) {
            written = snprintf(line,
                               sizeof(line),
                               "%s\"%s\": null%s\n",
                               indent,
                               g_metric_table[i].json_key,
                               last ? "" : ",");
        } else {
            value = assessment->results[index];
            written = snprintf(line,
                               sizeof(line),
                               "%s\"%s\": %.6f%s\n",
                               indent,
                               g_metric_table[i].json_key,
                               value,
                               last ? "" : ",");
        }
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);
    }
    return SIXEL_OK;
}

/* Strings come from CLI input, so escape each value before emitting it. */
static SIXELSTATUS
assessment_emit_color_context(sixel_assessment_t *assessment,
                                sixel_assessment_json_callback_t callback,
                                void *user_data)
{
    char escaped[SIXEL_ASSESSMENT_PATH_CAPACITY * 6 + 1];
    char line[SIXEL_ASSESSMENT_PATH_CAPACITY * 6 + 1024];
    char const *cms;
    int i;
    int n;

    cms = getenv("SIXEL_LOADER_CMS_ENGINE");
    if (assessment_escape_json(cms == NULL ? "auto" : cms,
                               escaped, sizeof(escaped)) != 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    n = snprintf(line, sizeof(line),
        "  \"color_evaluation\": {\n"
        "    \"pcs\": \"XYZ D50, Y=1\",\n"
        "    \"lab_units\": \"CIE L*, a*, b*\",\n"
        "    \"evaluator\": \"builtin\",\n"
        "    \"requested_loader_cms\": \"%s\",\n"
        "    \"plain_sixel_default\": \"srgb\",\n"
        "    \"requested_sixel_drawing\": \"%s\",\n"
        "    \"background_definition\": \"srgb\",\n"
        "    \"background_rgb8\": [%u, %u, %u],\n"
        "    \"signal\": \"%s\",\n"
        "    \"signal_precision\": \"%s\",\n"
        "    \"signal_range\": \"%s\",\n"
        "    \"pcs_precision\": \"float64 from current samples\",\n"
        "    \"grayscale_signal\": %s,\n"
        "    \"scielab_ppd\": %.17g,\n"
        "    \"scielab_model\": \"zhang-wandell-1998-unit-dc\",\n"
        "    \"scielab_difference\": \"CIE76\",\n"
        "    \"scielab_boundary\": \"edge-repeating reflection\",\n"
        "    \"region_window\": %d,\n"
        "    \"region_count\": %zu,\n"
        "    \"region_layout\": \"top-left fixed tiles, partial edges\",\n"
        "    \"region_mean_weighting\": \"pixel area\",\n",
        escaped, assessment->direct ? "direct" : "indexed",
        (unsigned int)assessment->background[0],
        (unsigned int)assessment->background[1],
        (unsigned int)assessment->background[2],
        assessment->signal_space == SIXEL_COLORSPACE_GAMMA
            ? "encoded sRGB luma (0.2126,0.7152,0.0722)"
            : "linear sRGB Y (0.2126,0.7152,0.0722)",
        assessment->signal_bytes ? "8bit" : "float32",
        assessment->signal_bytes ? "0..1" : "extended, nominal 0..1",
        assessment->grayscale ? "true" : "false",
        assessment->scielab_ppd,
        assessment->region_window, assessment->region_count);
    if (n < 0 || (size_t)n >= sizeof(line)) {
        return SIXEL_RUNTIME_ERROR;
    }
    callback(line, (size_t)n, user_data);
    if ((assessment->results_valid_mask & SIXEL_ASSESSMENT_METRIC_MASK(
            SIXEL_ASSESSMENT_METRIC_DELTA_E00_MAX)) != 0u) {
        n = snprintf(line, sizeof(line),
            "    \"pixel_delta_e00\": {\n"
            "      \"maximum_xy\": [%d, %d],\n"
            "      \"strictly_above\": {\"1\": %zu, \"5\": %zu, "
            "\"10\": %zu, \"20\": %zu, \"50\": %zu}\n"
            "    },\n",
            assessment->delta_e00_max_x, assessment->delta_e00_max_y,
            assessment->delta_e00_above[0], assessment->delta_e00_above[1],
            assessment->delta_e00_above[2], assessment->delta_e00_above[3],
            assessment->delta_e00_above[4]);
    } else {
        n = snprintf(line, sizeof(line),
                     "    \"pixel_delta_e00\": null,\n");
    }
    if (n < 0 || (size_t)n >= sizeof(line)) {
        return SIXEL_RUNTIME_ERROR;
    }
    callback(line, (size_t)n, user_data);
    for (i = 0; i < 2; ++i) {
        if (assessment_escape_json(assessment->color_spec[i], escaped,
                                   sizeof(escaped)) != 0) {
            return SIXEL_BAD_ARGUMENT;
        }
        n = snprintf(line, sizeof(line),
            "    \"%s\": {\n"
            "      \"requested_definition\": \"%s\",\n"
            "      \"origin\": \"%s\",\n"
            "      \"drawing\": \"%s\",\n"
            "      \"current_basis\": \"srgb\",\n"
            "      \"current_coordinate\": \"%s\",\n"
            "      \"current_format\": \"%s\"\n"
            "    }%s\n",
            i == 0 ? "reference" : "target", escaped,
            assessment->color_origin[i], assessment->drawing[i],
            sixel_assessment_colorspace_name(assessment->current_space[i]),
            assessment_pixelformat_name(assessment->current_format[i]),
            i == 0 ? "," : "");
        if (n < 0 || (size_t)n >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)n, user_data);
    }
    callback("  },\n", 5, user_data);
    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_assessment_get_json(sixel_assessment_t *assessment,
                          unsigned int sections,
                          sixel_assessment_json_callback_t callback,
                          void *user_data)
{
    enum { path_buffer_size = SIXEL_ASSESSMENT_PATH_CAPACITY * 2 + 32 };
    enum { line_buffer_size = SIXEL_ASSESSMENT_PATH_CAPACITY * 2 + 128 };
    unsigned int requested_sections;
    unsigned int remaining_sections;
    unsigned int stored_sections;
    unsigned int requested_view;
    unsigned int stored_view;
    size_t stage_count;
    size_t stage_index;
    sixel_assessment_stage_t stage;
    static SIXEL_ASSESSMENT_TLS char line[line_buffer_size];
    static SIXEL_ASSESSMENT_TLS char escaped_path[path_buffer_size];
    static SIXEL_ASSESSMENT_TLS char escaped_loader[128];
    static SIXEL_ASSESSMENT_TLS char escaped_format[64];
    double duration_ms;
    double throughput;
    double total_duration;
    double ratio_source;
    double ratio_quantized;
    double seconds;
    size_t bytes;
    int is_last_stage;
    int written;
    int has_more;
    size_t next_index;
    SIXELSTATUS status;

    if (assessment == NULL || callback == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    requested_sections = sections & SIXEL_ASSESSMENT_SECTION_MASK;
    if (requested_sections == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }
    stored_sections = assessment->sections_mask;
    if ((requested_sections & stored_sections) != requested_sections) {
        return SIXEL_RUNTIME_ERROR;
    }
    requested_view = sections & SIXEL_ASSESSMENT_VIEW_MASK;
    stored_view = assessment->view_mask & SIXEL_ASSESSMENT_VIEW_MASK;
    if (requested_view == 0u) {
        requested_view = stored_view;
    }
    if ((requested_sections & SIXEL_ASSESSMENT_SECTION_QUALITY) != 0u) {
        if (!assessment->results_ready) {
            return SIXEL_RUNTIME_ERROR;
        }
        if (requested_view != stored_view) {
            return SIXEL_BAD_ARGUMENT;
        }
    }

    callback("{\n", sizeof("{\n") - 1, user_data);
    remaining_sections = requested_sections;
    if ((requested_sections & SIXEL_ASSESSMENT_SECTION_QUALITY) != 0u) {
        status = assessment_emit_color_context(assessment, callback, user_data);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }


    if ((requested_sections & SIXEL_ASSESSMENT_SECTION_BASIC) != 0u) {
        assessment_guess_format(assessment);
        if (assessment_escape_json(assessment->input_path,
                                   escaped_path,
                                   sizeof(escaped_path)) != 0) {
            return SIXEL_RUNTIME_ERROR;
        }
        if (assessment_escape_json(assessment->loader_name,
                                   escaped_loader,
                                   sizeof(escaped_loader)) != 0) {
            return SIXEL_RUNTIME_ERROR;
        }
        if (assessment_escape_json(assessment->format_name,
                                   escaped_format,
                                   sizeof(escaped_format)) != 0) {
            return SIXEL_RUNTIME_ERROR;
        }

        written = snprintf(line,
                           sizeof(line),
                           "  \"basic\": {\n");
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"input_path\": \"%s\",\n",
                           escaped_path);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"input_format\": \"%s\",\n",
                           escaped_format);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"loader\": \"%s\",\n",
                           escaped_loader);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"input_bytes\": %zu,\n",
                           assessment->input_bytes);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"input_pixelformat\": \"%s\",\n",
                           assessment_pixelformat_name(
                               assessment->input_pixelformat));
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"input_colorspace\": \"%s\",\n",
                           sixel_assessment_colorspace_name(
                               assessment->input_colorspace));
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"source_pixels_bytes\": %zu,\n",
                           assessment->source_pixels_bytes);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"quantized_pixels_bytes\": %zu,\n",
                           assessment->quantized_pixels_bytes);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"palette_colors\": %d,\n",
                           assessment->palette_colors);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"palette_bytes\": %zu\n",
                           assessment->palette_bytes);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        remaining_sections &= ~SIXEL_ASSESSMENT_SECTION_BASIC;
        has_more = (remaining_sections != 0u);
        callback(has_more ? "  },\n" : "  }\n",
                 has_more ? sizeof("  },\n") - 1 : sizeof("  }\n") - 1,
                 user_data);
    }

    if ((requested_sections & SIXEL_ASSESSMENT_SECTION_PERFORMANCE) != 0u) {
        stage_count = sizeof(g_stage_descriptors)
                    / sizeof(g_stage_descriptors[0]);
        written = snprintf(line,
                           sizeof(line),
                           "  \"performance\": {\n");
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        callback("    \"stages\": [\n",
                 sizeof("    \"stages\": [\n") - 1,
                 user_data);

        total_duration = 0.0;
        stage_index = 0;
        while (stage_index < stage_count) {
            if (!sixel_assessment_stage_should_emit(stage_index)) {
                ++stage_index;
                continue;
            }
            stage = g_stage_descriptors[stage_index].id;
            seconds = assessment->stage_durations[stage];
            bytes = (size_t)assessment->stage_bytes[stage];
            if (g_stage_counts_toward_total[stage_index] != 0) {
                total_duration += seconds;
            }
            duration_ms = seconds * 1000.0;
            if (seconds > 0.0) {
                throughput = (double)bytes / seconds;
            } else {
                throughput = 0.0;
            }

            next_index = stage_index + 1u;
            while (next_index < stage_count
                    && !sixel_assessment_stage_should_emit(next_index)) {
                ++next_index;
            }
            is_last_stage = next_index >= stage_count ? 1 : 0;

            written = snprintf(line,
                               sizeof(line),
                               "      {\n"
                               "        \"name\": \"%s\",\n"
                               "        \"duration_ms\": %.6f,\n",
                               g_stage_descriptors[stage_index].label,
                               duration_ms);
            if (written < 0 || (size_t)written >= sizeof(line)) {
                return SIXEL_RUNTIME_ERROR;
            }
            callback(line, (size_t)written, user_data);

            if (stage == SIXEL_ASSESSMENT_STAGE_ENCODE &&
                    g_encode_parallel_threads > 1) {
                written = snprintf(line,
                                   sizeof(line),
                                   "        \"parallel_threads\": %d,\n",
                                   g_encode_parallel_threads);
                if (written < 0 || (size_t)written >= sizeof(line)) {
                    return SIXEL_RUNTIME_ERROR;
                }
                callback(line, (size_t)written, user_data);
            }

            written = snprintf(
                line,
                sizeof(line),
                "        \"bytes_processed\": %zu,\n"
                "        \"throughput_bytes_per_second\": %.6f\n"
                "      }%s\n",
                bytes,
                throughput,
                is_last_stage ? "" : ",");
            if (written < 0 || (size_t)written >= sizeof(line)) {
                return SIXEL_RUNTIME_ERROR;
            }
            callback(line, (size_t)written, user_data);

            stage_index = next_index;
        }

        callback("    ],\n", sizeof("    ],\n") - 1, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"total_duration_ms\": %.6f\n",
                           total_duration * 1000.0);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        remaining_sections &= ~SIXEL_ASSESSMENT_SECTION_PERFORMANCE;
        has_more = (remaining_sections != 0u);
        callback(has_more ? "  },\n" : "  }\n",
                 has_more ? sizeof("  },\n") - 1 : sizeof("  }\n") - 1,
                 user_data);
    }

    if ((requested_sections & SIXEL_ASSESSMENT_SECTION_SIZE) != 0u) {
        ratio_source = 0.0;
        if (assessment->source_pixels_bytes > 0) {
            ratio_source = (double)assessment->output_bytes /
                (double)assessment->source_pixels_bytes;
        }
        ratio_quantized = 0.0;
        if (assessment->quantized_pixels_bytes > 0) {
            ratio_quantized = (double)assessment->output_bytes /
                (double)assessment->quantized_pixels_bytes;
        }

        written = snprintf(line,
                           sizeof(line),
                           "  \"size\": {\n");
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"output_bytes\": %zu,\n",
                           assessment->output_bytes);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"output_vs_source_ratio\": %.6f,\n",
                           ratio_source);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        written = snprintf(line,
                           sizeof(line),
                           "    \"output_vs_quantized_ratio\": %.6f\n",
                           ratio_quantized);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        remaining_sections &= ~SIXEL_ASSESSMENT_SECTION_SIZE;
        has_more = (remaining_sections != 0u);
        callback(has_more ? "  },\n" : "  }\n",
                 has_more ? sizeof("  },\n") - 1 : sizeof("  }\n") - 1,
                 user_data);
    }

    if ((requested_sections & SIXEL_ASSESSMENT_SECTION_QUALITY) != 0u) {
        written = snprintf(line,
                           sizeof(line),
                           "  \"quality\": {\n");
        if (written < 0 || (size_t)written >= sizeof(line)) {
            return SIXEL_RUNTIME_ERROR;
        }
        callback(line, (size_t)written, user_data);

        status = assessment_emit_quality_lines(assessment,
                                               callback,
                                               user_data,
                                               "    ");
        if (SIXEL_FAILED(status)) {
            return status;
        }

        remaining_sections &= ~SIXEL_ASSESSMENT_SECTION_QUALITY;
        has_more = (remaining_sections != 0u);
        callback(has_more ? "  },\n" : "  }\n",
                 has_more ? sizeof("  },\n") - 1 : sizeof("  }\n") - 1,
                 user_data);
    }

    callback("}\n", sizeof("}\n") - 1, user_data);
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
