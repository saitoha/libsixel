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

#ifndef SIXEL_SRC_ASSESSMENT_H
#define SIXEL_SRC_ASSESSMENT_H

#include <limits.h>
#include <setjmp.h>
#include <stddef.h>

#include <sixel.h>

#include "sixel_atomic.h"

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

struct sixel_encoder;
struct sixel_assessment;
typedef struct sixel_assessment sixel_assessment_t;

typedef enum sixel_assessment_mode {
    SIXEL_ASSESSMENT_MODE_NONE = 0,
    SIXEL_ASSESSMENT_MODE_QUANTIZED,
    SIXEL_ASSESSMENT_MODE_ENCODED
} sixel_assessment_mode_t;

typedef void (*sixel_assessment_json_callback_t)(
    char const *chunk,
    size_t length,
    void *user_data);

#define SIXEL_ASSESSMENT_SECTION_NONE        0x00000000u
#define SIXEL_ASSESSMENT_SECTION_BASIC       0x00000001u
#define SIXEL_ASSESSMENT_SECTION_PERFORMANCE 0x00000002u
#define SIXEL_ASSESSMENT_SECTION_SIZE        0x00000004u
#define SIXEL_ASSESSMENT_SECTION_QUALITY     0x00000008u
#define SIXEL_ASSESSMENT_SECTION_MASK        0x0000000fu
#define SIXEL_ASSESSMENT_SECTION_ALL \
    (SIXEL_ASSESSMENT_SECTION_BASIC | \
     SIXEL_ASSESSMENT_SECTION_PERFORMANCE | \
     SIXEL_ASSESSMENT_SECTION_SIZE | \
     SIXEL_ASSESSMENT_SECTION_QUALITY)
#define SIXEL_ASSESSMENT_VIEW_ENCODED        0x00000000u
#define SIXEL_ASSESSMENT_VIEW_QUANTIZED      0x00010000u
#define SIXEL_ASSESSMENT_VIEW_MASK           0x00010000u

#define SIXEL_ASSESSMENT_KIND_RELATIVE   0x00010000
#define SIXEL_ASSESSMENT_KIND_ABSOLUTE   0x00020000
#define SIXEL_ASSESSMENT_SCOPE_COMPONENT 0x00001000
#define SIXEL_ASSESSMENT_SCOPE_COMPOSITE 0x00002000
#define SIXEL_ASSESSMENT_INDEX_MASK      0x00000fff
#define SIXEL_ASSESSMENT_INDEX(value) \
    ((value) & SIXEL_ASSESSMENT_INDEX_MASK)
#define SIXEL_ASSESSMENT_METRIC_COUNT    25

#define SIXEL_ASSESSMENT_METRIC_MS_SSIM \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0000)
#define SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_OUT \
    (SIXEL_ASSESSMENT_KIND_ABSOLUTE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0001)
#define SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_REF \
    (SIXEL_ASSESSMENT_KIND_ABSOLUTE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0002)
#define SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_DELTA \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0003)
#define SIXEL_ASSESSMENT_METRIC_STRIPE_REF \
    (SIXEL_ASSESSMENT_KIND_ABSOLUTE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0004)
#define SIXEL_ASSESSMENT_METRIC_STRIPE_OUT \
    (SIXEL_ASSESSMENT_KIND_ABSOLUTE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0005)
#define SIXEL_ASSESSMENT_METRIC_STRIPE_REL \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0006)
#define SIXEL_ASSESSMENT_METRIC_BAND_RUN_REL \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0007)
#define SIXEL_ASSESSMENT_METRIC_BAND_GRAD_REL \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0008)
#define SIXEL_ASSESSMENT_METRIC_CLIP_L_REF \
    (SIXEL_ASSESSMENT_KIND_ABSOLUTE | \
     SIXEL_ASSESSMENT_SCOPE_COMPONENT | 0x0009)
#define SIXEL_ASSESSMENT_METRIC_CLIP_R_REF \
    (SIXEL_ASSESSMENT_KIND_ABSOLUTE | \
     SIXEL_ASSESSMENT_SCOPE_COMPONENT | 0x000a)
#define SIXEL_ASSESSMENT_METRIC_CLIP_G_REF \
    (SIXEL_ASSESSMENT_KIND_ABSOLUTE | \
     SIXEL_ASSESSMENT_SCOPE_COMPONENT | 0x000b)
#define SIXEL_ASSESSMENT_METRIC_CLIP_B_REF \
    (SIXEL_ASSESSMENT_KIND_ABSOLUTE | \
     SIXEL_ASSESSMENT_SCOPE_COMPONENT | 0x000c)
#define SIXEL_ASSESSMENT_METRIC_CLIP_L_OUT \
    (SIXEL_ASSESSMENT_KIND_ABSOLUTE | \
     SIXEL_ASSESSMENT_SCOPE_COMPONENT | 0x000d)
#define SIXEL_ASSESSMENT_METRIC_CLIP_R_OUT \
    (SIXEL_ASSESSMENT_KIND_ABSOLUTE | \
     SIXEL_ASSESSMENT_SCOPE_COMPONENT | 0x000e)
#define SIXEL_ASSESSMENT_METRIC_CLIP_G_OUT \
    (SIXEL_ASSESSMENT_KIND_ABSOLUTE | \
     SIXEL_ASSESSMENT_SCOPE_COMPONENT | 0x000f)
#define SIXEL_ASSESSMENT_METRIC_CLIP_B_OUT \
    (SIXEL_ASSESSMENT_KIND_ABSOLUTE | \
     SIXEL_ASSESSMENT_SCOPE_COMPONENT | 0x0010)
#define SIXEL_ASSESSMENT_METRIC_CLIP_L_REL \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPONENT | 0x0011)
#define SIXEL_ASSESSMENT_METRIC_CLIP_R_REL \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPONENT | 0x0012)
#define SIXEL_ASSESSMENT_METRIC_CLIP_G_REL \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPONENT | 0x0013)
#define SIXEL_ASSESSMENT_METRIC_CLIP_B_REL \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPONENT | 0x0014)
#define SIXEL_ASSESSMENT_METRIC_DELTA_CHROMA \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0015)
#define SIXEL_ASSESSMENT_METRIC_DELTA_E00 \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0016)
#define SIXEL_ASSESSMENT_METRIC_GMSD \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0017)
#define SIXEL_ASSESSMENT_METRIC_PSNR_Y \
    (SIXEL_ASSESSMENT_KIND_RELATIVE | \
     SIXEL_ASSESSMENT_SCOPE_COMPOSITE | 0x0018)

#define SIXEL_ASSESSMENT_METRIC_MASK(metric_id) \
    (1u << SIXEL_ASSESSMENT_INDEX(metric_id))
#define SIXEL_ASSESSMENT_METRIC_MASK_ALL \
    (((SIXEL_ASSESSMENT_METRIC_COUNT) >= \
      (int)(sizeof(unsigned int) * 8u)) ? \
     0xffffffffu : \
     ((1u << (SIXEL_ASSESSMENT_METRIC_COUNT)) - 1u))

typedef enum sixel_assessment_stage {
    SIXEL_ASSESSMENT_STAGE_NONE = 0,
    SIXEL_ASSESSMENT_STAGE_IMAGE_CHUNK,
    SIXEL_ASSESSMENT_STAGE_IMAGE_DECODE,
    SIXEL_ASSESSMENT_STAGE_SCALE,
    SIXEL_ASSESSMENT_STAGE_CROP,
    SIXEL_ASSESSMENT_STAGE_COLORSPACE,
    SIXEL_ASSESSMENT_STAGE_PALETTE_HISTOGRAM,
    SIXEL_ASSESSMENT_STAGE_PALETTE_SOLVE,
    SIXEL_ASSESSMENT_STAGE_PALETTE_APPLY,
    SIXEL_ASSESSMENT_STAGE_ENCODE,
    SIXEL_ASSESSMENT_STAGE_ENCODE_PREPARE,
    SIXEL_ASSESSMENT_STAGE_ENCODE_CLASSIFY,
    SIXEL_ASSESSMENT_STAGE_ENCODE_COMPOSE,
    SIXEL_ASSESSMENT_STAGE_ENCODE_COMPOSE_SCAN,
    SIXEL_ASSESSMENT_STAGE_ENCODE_COMPOSE_QUEUE,
    SIXEL_ASSESSMENT_STAGE_ENCODE_EMIT,
    SIXEL_ASSESSMENT_STAGE_OUTPUT,
    SIXEL_ASSESSMENT_STAGE_COUNT
} sixel_assessment_stage_t;

typedef enum sixel_assessment_spool_mode {
    SIXEL_ASSESSMENT_SPOOL_MODE_NONE = 0,
    SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT,
    SIXEL_ASSESSMENT_SPOOL_MODE_PATH
} sixel_assessment_spool_mode_t;

#if defined(_MSC_VER)
#pragma warning(push)
/*
 * jmp_buf may request extended alignment on MSVC; suppress padding noise so
 * we can keep the struct layout near the portable definition.
 */
#pragma warning(disable : 4324)
#endif  /* _MSC_VER */

struct sixel_assessment {
    sixel_atomic_u32_t refcount;
    sixel_allocator_t *allocator;
    int results_ready;
    SIXELSTATUS last_error;
    jmp_buf bailout;
    char error_message[256];
    unsigned int results_valid_mask; /* valid bits for results[] */
    double results[SIXEL_ASSESSMENT_METRIC_COUNT];
    sixel_assessment_stage_t active_stage;
    double stage_started_at;
    int stage_active;
    double stage_durations[SIXEL_ASSESSMENT_STAGE_COUNT];
    double stage_bytes[SIXEL_ASSESSMENT_STAGE_COUNT];
    double encode_output_time_pending; /* pending write spans during encode */
    double encode_palette_time_pending; /* pending palette spans during encode */
    char input_path[PATH_MAX];
    char loader_name[64];
    char format_name[32];
    int input_pixelformat;
    int input_colorspace;
    size_t input_bytes;
    size_t source_pixels_bytes;
    size_t quantized_pixels_bytes;
    size_t palette_bytes;
    int palette_colors;
    size_t output_bytes;
    size_t output_bytes_written;
    unsigned int metrics_mask;
    unsigned int sections_mask;
    unsigned int view_mask;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif  /* _MSC_VER */

SIXEL_INTERNAL_API SIXELSTATUS
sixel_assessment_new(
    sixel_assessment_t  **ppassessment,
    sixel_allocator_t   *allocator);

SIXEL_INTERNAL_API void
sixel_assessment_ref(sixel_assessment_t *assessment);

SIXEL_INTERNAL_API void
sixel_assessment_unref(sixel_assessment_t *assessment);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_assessment_setopt(sixel_assessment_t *assessment,
                        int option,
                        char const *value);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_assessment_analyze(sixel_assessment_t *assessment,
                         sixel_frame_t *reference,
                         sixel_frame_t *output);

/*
 *  +---------------------------+
 *  | Local call graph          |
 *  +---------------------------+
 *  | lsqa -> assessment core   |
 *  +---------------------------+
 *
 *  These helpers live in a private header so the lsqa tool can reuse
 *  the quality pipeline without exposing the API in the public header.
 */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_assessment_expand_quantized_frame(sixel_frame_t *source,
                                        sixel_allocator_t *allocator,
                                        sixel_frame_t **ppframe);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_assessment_load_single_frame(char const *path,
                                   sixel_allocator_t *allocator,
                                   sixel_frame_t **ppframe);

SIXEL_INTERNAL_API void
sixel_assessment_stage_transition(sixel_assessment_t *assessment,
                                  sixel_assessment_stage_t stage);

SIXEL_INTERNAL_API void
sixel_assessment_stage_finish(sixel_assessment_t *assessment);

SIXEL_INTERNAL_API void
sixel_assessment_record_stage_duration(sixel_assessment_t *assessment,
                                       sixel_assessment_stage_t stage,
                                       double duration);

SIXEL_INTERNAL_API void
sixel_assessment_record_loader(sixel_assessment_t *assessment,
                               char const *path,
                               char const *loader_name,
                               size_t input_bytes);

SIXEL_INTERNAL_API void
sixel_assessment_record_source_frame(sixel_assessment_t *assessment,
                                     sixel_frame_t *frame);

SIXEL_INTERNAL_API void
sixel_assessment_record_quantized_capture(
    sixel_assessment_t *assessment,
    struct sixel_encoder *encoder);

SIXEL_INTERNAL_API void
sixel_assessment_record_output_size(sixel_assessment_t *assessment,
                                    size_t output_bytes);

SIXEL_INTERNAL_API void
sixel_assessment_record_output_write(sixel_assessment_t *assessment,
                                     size_t bytes,
                                     double duration);

SIXEL_INTERNAL_API int
sixel_assessment_palette_probe_enabled(void);

SIXEL_INTERNAL_API void
sixel_assessment_record_palette_apply_span(double duration);

SIXEL_INTERNAL_API int
sixel_assessment_encode_probe_enabled(void);

SIXEL_INTERNAL_API void
sixel_assessment_set_encode_parallelism(int threads);

SIXEL_INTERNAL_API void
sixel_assessment_record_encode_span(sixel_assessment_stage_t stage,
                                    double duration);

SIXEL_INTERNAL_API void
sixel_assessment_record_encode_work(sixel_assessment_stage_t stage,
                                    double amount);

SIXEL_INTERNAL_API void
sixel_assessment_select_metrics(sixel_assessment_t *assessment,
                                unsigned int metrics);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_assessment_get_json(sixel_assessment_t *assessment,
                          unsigned int sections,
                          sixel_assessment_json_callback_t callback,
                          void *user_data);

SIXEL_INTERNAL_API void
sixel_assessment_select_sections(sixel_assessment_t *assessment,
                                 unsigned int sections);

SIXEL_INTERNAL_API void
sixel_assessment_attach_encoder(sixel_assessment_t *assessment,
                                struct sixel_encoder *encoder);

#endif /* SIXEL_SRC_ASSESSMENT_H */
