/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
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

#ifndef LIBSIXEL_ENCODER_H
#define LIBSIXEL_ENCODER_H

#include <stddef.h>
#include <limits.h>

#include "sixel_atomic.h"

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

struct sixel_frame;
struct sixel_logger;

#include "planner.h"

SIXEL_INTERNAL_API int
sixel_encoder_should_hide_animation_cursor(int is_multiframe,
                                           int fstatic,
                                           int outfd_is_tty,
                                           char const *env_value);

/* palette type */
#define SIXEL_COLOR_OPTION_DEFAULT          0   /* use default settings */
#define SIXEL_COLOR_OPTION_MONOCHROME       1   /* use monochrome palette */
#define SIXEL_COLOR_OPTION_BUILTIN          2   /* use builtin palette */
#define SIXEL_COLOR_OPTION_MAPFILE          3   /* use mapfile option */
#define SIXEL_COLOR_OPTION_HIGHCOLOR        4   /* use highcolor option */

/* encoder object */
struct sixel_encoder {

    sixel_atomic_u32_t ref;         /* reference counter */
    sixel_allocator_t *allocator;   /* allocator object */
    int reqcolors;
    size_t palette_sample_target;   /* target sample count for palette */
    int palette_sample_override;    /* non-zero when env requested target */
    int force_palette;              /* force palette size when non-zero */
    int color_option;
    char *mapfile;
    char *palette_output;
    char *loader_order;
    int loader_wic_ico_minsize;
    int loader_start_frame_no;
    int loader_start_frame_no_set;
    int builtin_palette;
    int method_for_diffuse;
    int interframe_strategy_override;
    int interframe_strategy_token;
    int interframe_noise_strength_override;
    int interframe_noise_strength_u8;
    int method_for_scan;
    int method_for_carry;
    int method_for_largest;
    int method_for_rep;
    int quality_mode;
    int quantize_model;
    int quantize_model_kmeans_init_override;
    int quantize_model_kmeans_init_type;
    int quantize_model_kmeans_threshold_override;
    double quantize_model_kmeans_threshold;
    int quantize_model_kmeans_binning_override;
    int quantize_model_kmeans_binning_mode;
    int quantize_model_kmeans_binbits_override;
    unsigned int quantize_model_kmeans_binbits;
    int quantize_model_kmeans_mapping_override;
    int quantize_model_kmeans_mapping_mode;
    int quantize_model_kmeans_softdist_override;
    int quantize_model_kmeans_softdist_mode;
    int quantize_model_kmeans_autoratio_override;
    unsigned int quantize_model_kmeans_autoratio;
    int quantize_model_kmeans_feedback_override;
    int quantize_model_kmeans_feedback_mode;
    int quantize_model_kmeans_seed_override;
    unsigned int quantize_model_kmeans_seed;
    int quantize_model_kmeans_restarts_override;
    unsigned int quantize_model_kmeans_restarts;
    int quantize_model_kmeans_iter_override;
    unsigned int quantize_model_kmeans_iter;
    int quantize_model_kmeans_iter_max_override;
    unsigned int quantize_model_kmeans_iter_max;
    int quantize_model_kmeans_miniter_override;
    unsigned int quantize_model_kmeans_miniter;
    int quantize_model_kmeans_polish_iter_override;
    unsigned int quantize_model_kmeans_polish_iter;
    int quantize_model_kmeans_feedback_slots_override;
    unsigned int quantize_model_kmeans_feedback_slots;
    int quantize_model_kmeans_feedback_interval_override;
    unsigned int quantize_model_kmeans_feedback_interval;
    int quantize_model_kmedoids_algo_override;
    int quantize_model_kmedoids_algo;
    int quantize_model_kmedoids_seed_override;
    unsigned int quantize_model_kmedoids_seed;
    int quantize_model_kmedoids_iter_override;
    unsigned int quantize_model_kmedoids_iter;
    int quantize_model_kmedoids_sample_override;
    unsigned int quantize_model_kmedoids_sample;
    int quantize_model_kmedoids_clara_trials_override;
    unsigned int quantize_model_kmedoids_clara_trials;
    int quantize_model_kmedoids_clara_sample_override;
    unsigned int quantize_model_kmedoids_clara_sample;
    int quantize_model_kmedoids_clarans_local_override;
    unsigned int quantize_model_kmedoids_clarans_local;
    int quantize_model_kmedoids_clarans_neighbors_override;
    unsigned int quantize_model_kmedoids_clarans_neighbors;
    int quantize_model_kmedoids_bandit_iter_override;
    unsigned int quantize_model_kmedoids_bandit_iter;
    int quantize_model_kmedoids_bandit_candidates_override;
    unsigned int quantize_model_kmedoids_bandit_candidates;
    int quantize_model_kmedoids_bandit_batch_override;
    unsigned int quantize_model_kmedoids_bandit_batch;
    int quantize_model_kmedoids_histbits_override;
    unsigned int quantize_model_kmedoids_histbits;
    int quantize_model_kmedoids_point_budget_override;
    unsigned int quantize_model_kmedoids_point_budget;
    int quantize_model_kmedoids_rare_keep_override;
    unsigned int quantize_model_kmedoids_rare_keep;
    int quantize_model_kmedoids_prune_mass_override;
    double quantize_model_kmedoids_prune_mass;
    int quantize_model_merge_override;
    int quantize_model_merge_mode;
    int quantize_model_merge_oversplit_override;
    double quantize_model_merge_oversplit;
    int quantize_model_merge_lloyd_override;
    unsigned int quantize_model_merge_lloyd;
    int final_merge_mode;
    int lut_policy;
    int sixel_reversible;
    int method_for_resampling;
    int loop_mode;
    int palette_type;
    int f8bit;
    int finvert;
    int fuse_macro;
    int fdrcs;
    int fignore_delay;
    int complexion;
    int fstatic;
    int cell_width;
    int cell_height;
    int pixelwidth;
    int pixelheight;
    int percentwidth;
    int percentheight;
    int clipx;
    int clipy;
    int clipwidth;
    int clipheight;
    int clipfirst;
    int macro_number;
    int penetrate_multiplexer;
    int encode_policy;
    int clustering_colorspace;
    int working_colorspace;
    int working_colorspace_set;
    int clustering_colorspace_set;
    int force_float32_colorspace;
    int output_colorspace;
    int prefer_float32;
    int ormode;
    int pipe_mode;
    int verbose;
    int has_gri_arg_limit;
    unsigned char *bgcolor;
    int outfd;
    int tile_outfd;
    int finsecure;
    int *cancel_flag;
    void *dither_cache;
    unsigned short drcs_charset_no;
    int drcs_mmv;
    int capture_quantized;
    int capture_source;
    unsigned char *capture_pixels;
    size_t capture_pixels_size;
    unsigned char *capture_palette;
    size_t capture_palette_size;
    size_t capture_pixel_bytes;
    int capture_width;
    int capture_height;
    int capture_pixelformat;
    int capture_colorspace;
    int capture_ncolors;
    int capture_valid;
    struct sixel_frame *capture_source_frame;
    char last_loader_name[64];
    char last_source_path[PATH_MAX];
    size_t last_input_bytes;
    int output_is_png;
    int output_png_to_stdout;
    char *png_output_path;
    char *sixel_output_path;
    int clipboard_output_active;
    char clipboard_output_format[32];
    char *clipboard_output_path;
    struct sixel_logger *logger;
    int parallel_job_id;
    int palette_job_enabled;
    sixel_encoding_planner_t planner;
};


#endif /* LIBSIXEL_ENCODER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
