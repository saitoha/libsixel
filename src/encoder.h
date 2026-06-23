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

#include "sixel_atomic.h"
#include "timeline-logger.h"

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
    int interframe_spatial_diffuse_override;
    int interframe_spatial_diffuse;
    int interframe_noise_strength_override;
    int interframe_noise_strength_u8;
    int stbn_motion_adapt_override;
    int stbn_motion_adapt_enabled;
    int stbn_scene_cut_reset_override;
    int stbn_scene_cut_reset_enabled;
    int stbn_scene_detect_override;
    int stbn_scene_detect_enabled;
    int stbn_alpha_guard_override;
    int stbn_alpha_guard_enabled;
    int stbn_perceptual_weight_override;
    int stbn_perceptual_weight_enabled;
    int stbn_fastpath_override;
    int stbn_fastpath_enabled;
    int bluenoise_strength_override;
    float bluenoise_strength;
    int bluenoise_phase_override;
    int bluenoise_phase_x;
    int bluenoise_phase_y;
    int bluenoise_seed_override;
    int bluenoise_seed;
    int bluenoise_channel_override;
    int bluenoise_channel_rgb;
    int bluenoise_size_override;
    int bluenoise_size;
    int bluenoise_gradient_factor_override;
    float bluenoise_gradient_factor;
    int method_for_scan;
    int method_for_largest;
    int method_for_largest_override;
    int method_for_rep;
    int quality_mode;
    int quantize_model;
    int quantize_model_heckbert_profile;
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
    int quantize_model_kmeans_prune_override;
    int quantize_model_kmeans_prune_policy;
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
    int quantize_model_kmedoids_auction_override;
    int quantize_model_kmedoids_auction;
    int quantize_model_kmedoids_auction_shortlist_override;
    unsigned int quantize_model_kmedoids_auction_shortlist;
    int quantize_model_kcenter_algo_override;
    int quantize_model_kcenter_algo;
    int quantize_model_kcenter_seed_override;
    unsigned int quantize_model_kcenter_seed;
    int quantize_model_kcenter_restarts_override;
    unsigned int quantize_model_kcenter_restarts;
    int quantize_model_kcenter_init_seeds_override;
    unsigned int quantize_model_kcenter_init_seeds;
    int quantize_model_kcenter_iter_override;
    unsigned int quantize_model_kcenter_iter;
    int quantize_model_kcenter_histbits_override;
    unsigned int quantize_model_kcenter_histbits;
    int quantize_model_kcenter_point_budget_override;
    unsigned int quantize_model_kcenter_point_budget;
    int quantize_model_kcenter_prune_mass_override;
    double quantize_model_kcenter_prune_mass;
    int quantize_model_kcenter_profile_override;
    int quantize_model_kcenter_profile;
    int quantize_model_kcenter_auto_policy_override;
    int quantize_model_kcenter_auto_policy;
    int quantize_model_kcenter_auto_fft_threshold_override;
    unsigned int quantize_model_kcenter_auto_fft_threshold;
    int quantize_model_kcenter_space_policy_override;
    int quantize_model_kcenter_space_policy;
    int quantize_model_kcenter_candidate_policy_override;
    int quantize_model_kcenter_candidate_policy;
    int quantize_model_kcenter_rare_keep_override;
    unsigned int quantize_model_kcenter_rare_keep;
    int quantize_model_kcenter_budget_policy_override;
    int quantize_model_kcenter_budget_policy;
    int quantize_model_kcenter_budget_scale_override;
    double quantize_model_kcenter_budget_scale;
    int quantize_model_kcenter_swap_topk_override;
    unsigned int quantize_model_kcenter_swap_topk;
    int quantize_model_kcenter_swap_update_override;
    int quantize_model_kcenter_swap_update;
    int quantize_model_kcenter_swap_patience_override;
    unsigned int quantize_model_kcenter_swap_patience;
    int quantize_model_kcenter_swap_min_gain_override;
    double quantize_model_kcenter_swap_min_gain;
    int quantize_model_merge_override;
    int quantize_model_merge_mode;
    int quantize_model_merge_oversplit_override;
    double quantize_model_merge_oversplit;
    int quantize_model_merge_lloyd_override;
    unsigned int quantize_model_merge_lloyd;
    int quantize_model_animation_mode_override;
    int quantize_model_animation_mode;
    int quantize_model_scene_cut_threshold_override;
    double quantize_model_scene_cut_threshold;
    unsigned char quantize_animation_prev_palette[SIXEL_PALETTE_MAX * 3];
    float quantize_animation_prev_palette_float[SIXEL_PALETTE_MAX * 4];
    unsigned int quantize_animation_prev_palette_count;
    int quantize_animation_prev_palette_valid;
    int quantize_animation_prev_palette_float_valid;
    int quantize_animation_prev_palette_float_stride;
    unsigned char quantize_animation_prev_probe[192];
    int quantize_animation_prev_probe_valid;
    int quantize_animation_prev_width;
    int quantize_animation_prev_height;
    int final_merge_mode;
    int lut_policy;
    int lut_policy_override;
    int lut_policy_shared_instance_override;
    int lut_policy_shared_instance;
    int sixel_reversible;
    int method_for_resampling;
    int loop_mode;
    int palette_type;
    int f8bit;
    int finvert;
    int fuse_macro;
    int fdrcs;
    int fignore_delay;
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
    int transparent_policy;
    int transparent_policy_override;
    unsigned char *accumulation_pixels;
    size_t accumulation_pixels_size;
    unsigned char *accumulation_valid_mask;
    size_t accumulation_valid_mask_size;
    unsigned char *accumulation_mask;
    size_t accumulation_mask_size;
    int accumulation_width;
    int accumulation_height;
    int accumulation_pixelformat;
    unsigned int accumulation_delta;
    int accumulation_valid;
    int pipe_mode;
    int verbose;
    int has_gri_arg_limit;
    unsigned char *bgcolor;
    int bgcolor_source;
    int outfd;
    int tile_outfd;
    int finsecure;
    int *cancel_flag;
    sixel_cancel_function cancel_function;
    void *cancel_context;
    void *dither_cache;
    void *diagnostic_dither;
    unsigned short drcs_charset_no;
    int drcs_mmv;
    int capture_quantized;
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
    int output_is_png;
    int output_png_to_stdout;
    char *png_output_path;
    char *sixel_output_path;
    int clipboard_output_active;
    char clipboard_output_format[32];
    char *clipboard_output_path;
    sixel_timeline_logger_t *logger;
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
