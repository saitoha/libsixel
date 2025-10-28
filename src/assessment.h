/* SPDX-License-Identifier: MIT */

#ifndef SIXEL_SRC_ASSESSMENT_H
#define SIXEL_SRC_ASSESSMENT_H

#include <limits.h>
#include <setjmp.h>
#include <stddef.h>

#include <sixel.h>

#include "config.h"

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

struct sixel_encoder;

typedef enum sixel_assessment_stage {
    SIXEL_ASSESSMENT_STAGE_NONE = 0,
    SIXEL_ASSESSMENT_STAGE_ARGUMENT_PARSE,
    SIXEL_ASSESSMENT_STAGE_IMAGE_CHUNK,
    SIXEL_ASSESSMENT_STAGE_IMAGE_DECODE,
    SIXEL_ASSESSMENT_STAGE_SCALE,
    SIXEL_ASSESSMENT_STAGE_CROP,
    SIXEL_ASSESSMENT_STAGE_COLORSPACE,
    SIXEL_ASSESSMENT_STAGE_PALETTE_HISTOGRAM,
    SIXEL_ASSESSMENT_STAGE_PALETTE_SOLVE,
    SIXEL_ASSESSMENT_STAGE_PALETTE_APPLY,
    SIXEL_ASSESSMENT_STAGE_ENCODE,
    SIXEL_ASSESSMENT_STAGE_OUTPUT,
    SIXEL_ASSESSMENT_STAGE_COUNT
} sixel_assessment_stage_t;

typedef enum sixel_assessment_spool_mode {
    SIXEL_ASSESSMENT_SPOOL_MODE_NONE = 0,
    SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT,
    SIXEL_ASSESSMENT_SPOOL_MODE_PATH
} sixel_assessment_spool_mode_t;

struct sixel_assessment {
    int refcount;
    sixel_allocator_t *allocator;
    int enable_lpips;
    int results_ready;
    SIXELSTATUS last_error;
    jmp_buf bailout;
    char error_message[256];
    char binary_dir[PATH_MAX];
    int binary_dir_state;
    char model_dir[PATH_MAX];
    int model_dir_state;
    int lpips_models_ready;
    char diff_model_path[PATH_MAX];
    char feat_model_path[PATH_MAX];
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
    unsigned int sections_mask;
    unsigned int view_mask;
};

/*
 *  +---------------------------+
 *  | Export boundary diagram   |
 *  +---------------------------+
 *  | converters -> assessment  |
 *  +---------------------------+
 *  |  Windows link shim layer  |
 *  +---------------------------+
 *
 *  These helpers must be exported so that the converter binaries can
 *  reach the assessment pipeline when the DLL builds an import library.
 */
SIXELAPI SIXELSTATUS
sixel_assessment_expand_quantized_frame(sixel_frame_t *source,
                                        sixel_allocator_t *allocator,
                                        sixel_frame_t **ppframe);

SIXELAPI SIXELSTATUS
sixel_assessment_load_single_frame(char const *path,
                                   sixel_allocator_t *allocator,
                                   sixel_frame_t **ppframe);

double
sixel_assessment_timer_now(void);

void
sixel_assessment_stage_transition(sixel_assessment_t *assessment,
                                  sixel_assessment_stage_t stage);

void
sixel_assessment_stage_finish(sixel_assessment_t *assessment);

void
sixel_assessment_record_stage_duration(sixel_assessment_t *assessment,
                                       sixel_assessment_stage_t stage,
                                       double duration);

void
sixel_assessment_record_loader(sixel_assessment_t *assessment,
                               char const *path,
                               char const *loader_name,
                               size_t input_bytes);

void
sixel_assessment_record_source_frame(sixel_assessment_t *assessment,
                                     sixel_frame_t *frame);

void
sixel_assessment_record_quantized_capture(
    sixel_assessment_t *assessment,
    struct sixel_encoder *encoder);

void
sixel_assessment_record_output_size(sixel_assessment_t *assessment,
                                    size_t output_bytes);

void
sixel_assessment_record_output_write(sixel_assessment_t *assessment,
                                     size_t bytes,
                                     double duration);

int
sixel_assessment_palette_probe_enabled(void);

void
sixel_assessment_record_palette_apply_span(double duration);

SIXELSTATUS
sixel_assessment_get_json(sixel_assessment_t *assessment,
                          unsigned int sections,
                          sixel_assessment_json_callback_t callback,
                          void *user_data);

void
sixel_assessment_select_sections(sixel_assessment_t *assessment,
                                 unsigned int sections);

void
sixel_assessment_attach_encoder(sixel_assessment_t *assessment,
                                struct sixel_encoder *encoder);

#endif /* SIXEL_SRC_ASSESSMENT_H */
