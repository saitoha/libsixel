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
};

SIXELSTATUS
sixel_assessment_expand_quantized_frame(sixel_frame_t *source,
                                        sixel_allocator_t *allocator,
                                        sixel_frame_t **ppframe);

SIXELSTATUS
sixel_assessment_load_single_frame(char const *path,
                                   sixel_allocator_t *allocator,
                                   sixel_frame_t **ppframe);

#endif /* SIXEL_SRC_ASSESSMENT_H */
