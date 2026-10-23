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
#include "threading.h"
#include "timeline-logger.h"

#include "planner.h"
#include "delta-residual.h"
#include <stdint.h>

/* The caller serializes encoder access. Returns an owned, unquantized
 * float frame and source-domain mask; failure publishes neither. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_prepare_repair(sixel_encoder_t *encoder,
                             sixel_frame_t const *source,
                             sixel_frame_t **prepared,
                             unsigned char **domain);

typedef struct sixel_repair_commit {
    size_t position;
    unsigned char rgb[3];
} sixel_repair_commit_t;

/* Already-owned transaction hooks. All records and colors are validated
 * before output admission; the successful commit cannot allocate or fail. */
SIXEL_INTERNAL_API void
sixel_encoder_commit_repair(sixel_encoder_t *encoder,
    sixel_repair_commit_t const *records, size_t count);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_invalidate_6delta_plane_owned(sixel_encoder_t *encoder);

SIXEL_INTERNAL_API int
sixel_encoder_should_hide_animation_cursor(int is_multiframe,
                                           int fstatic,
                                           int outfd_is_tty,
                                           int enabled);

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
    uint64_t history_revision;
    uint64_t ordinary_revision;
    uint64_t configuration_revision;
    int revision_exhausted;
    struct sixel_progressive *progressive_context;
    /* Admission metadata uses the short mutex below. The admitted operation
     * owns the image/configuration state without holding this mutex, so a
     * waiting normal operation can register priority over repair work. */
    sixel_mutex_t operation_mutex;
    sixel_cond_t operation_cond;
    sixel_thread_identity_t operation_owner;
    unsigned int operation_waiters;
    int operation_initialized;
    int operation_active;
    int operation_emitting;
    /* File playback reserves configuration across loader/queue waits while
     * per-frame ownership can be released for progressive work. */
    int operation_playback;
    sixel_thread_identity_t playback_owner;
    /* A sixel_playback_t owns the encoder; ordinary calls are refused. */
    int playback_attached;
    struct sixel_encoder_callback_scope *active_callbacks;
    /* A setting candidate borrows runtime state. Destructive cache/history
     * effects are recorded here and run only after successful validation. */
    int option_staging;
    int option_clear_cache;
    int option_invalidate_history;
    int reqcolors;
    int palette_sampling_policy;  /* requested sampling-stage policy */
    int palette_sampling_override; /* non-zero when explicitly configured */
    int palette_binning_policy;   /* requested binning-stage policy */
    int palette_binning_override; /* non-zero when configured */
    int palette_binning_origin;   /* source of the configured policy */
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
    /* Paired FS coefficient perturbation; zero preserves classic weights. */
    double diffusion_deadzone;
    int diffusion_deadzone_override;
    float diffusion_perturb;
    int diffusion_perturb_override;
    int diffusion_perturb_seed;
    int diffusion_perturb_seed_override;
    int dither_parallel_band_overwrap_override;
    unsigned int dither_parallel_band_overwrap;
    int dither_parallel_band_width_override;
    unsigned int dither_parallel_band_width;
    int dither_parallel_threads_max_override;
    unsigned int dither_parallel_threads_max;
    int dither_pin_threads_override;
    int dither_pin_threads;
    int a_dither_strength_override;
    float a_dither_strength;
    int x_dither_strength_override;
    float x_dither_strength;
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
    int heckbert_cut;
    int heckbert_cut_override;
    int heckbert_order;
    int heckbert_order_override;
    double heckbert_weights[3];
    int heckbert_weights_override;
    int quantize_model_kmeans_backend_override;
    int quantize_model_kmeans_backend;
    int quantize_model_kmeans_init_override;
    int quantize_model_kmeans_init_type;
    int quantize_model_kmeans_threshold_override;
    double quantize_model_kmeans_threshold;
    int quantize_model_kmeans_binbits_override;
    unsigned int quantize_model_kmeans_binbits;
    int quantize_model_kmeans_mapping_override;
    int quantize_model_kmeans_mapping_mode;
    int quantize_model_kmeans_softdist_override;
    int quantize_model_kmeans_softdist_mode;
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
    int merge_policy_override;
    int final_merge_mode;
    int merge_policy_oversplit_override;
    double merge_policy_oversplit;
    int merge_policy_lloyd_override;
    unsigned int merge_policy_lloyd;
    int merge_policy_channel_factor_l_override;
    double merge_policy_channel_factor_l;
    /* Gamut anchoring applies after any quantizer, so it has its own axis. */
    int cover_policy_override;
    int cover_policy;
    int cover_policy_grow_override;
    int cover_policy_grow;
    int cover_policy_mode_override;
    int cover_policy_mode;
    /* Dither-only feedback control; never used by CMS or colorspaces. */
    int clamp_policy;
    int clamp_policy_override;
    double clamp_threshold;
    int clamp_threshold_override;
    int snap_policy_override;
    int snap_policy;
    int snap_policy_timing_override;
    int snap_policy_timing;
    int snap_policy_grid_override;
    int snap_policy_grid;
    int snap_policy_approach_rate_override;
    double snap_policy_approach_rate;
    int snap_policy_channel_factor_l_override;
    double snap_policy_channel_factor_l;
    int lut_policy;
    int lut_policy_override;
    int lut_policy_shared_instance_override;
    int lut_policy_shared_instance;
    int lut_policy_packing_override;
    int lut_policy_packing;
    int lut_policy_fhedt_resolution_override;
    int lut_policy_fhedt_resolution;
    int lut_policy_fhedt_refine_override;
    int lut_policy_fhedt_refine;
    int lut_policy_fhedt_weights_override;
    double lut_policy_fhedt_weights[3];
    int lut_policy_fhedt_shared_override;
    int lut_policy_fhedt_shared;
    int lut_policy_fhedt_use_dist2_override;
    int lut_policy_fhedt_use_dist2;
    int lut_policy_fhedt_use_cache_override;
    int lut_policy_fhedt_use_cache;
    int lut_policy_fhedt_tile_xy_override;
    unsigned int lut_policy_fhedt_tile_xy;
    int lut_policy_fhedt_tile_depth_override;
    unsigned int lut_policy_fhedt_tile_depth;
    int lut_policy_fhedt_first_touch_override;
    int lut_policy_fhedt_first_touch;
    int lut_policy_fhedt_pin_threads_override;
    int lut_policy_fhedt_pin_threads;
    int gpu_policy;
    int gpu_palette_threshold_override;
    size_t gpu_palette_threshold;
    int method_for_resampling;
    int loop_mode;
    int palette_type;
    int f8bit;
    int finvert;
    int fuse_macro;
    int fdrcs;
    int fignore_delay;
    int fstatic;
    int animation_hide_cursor;
    int animation_hide_cursor_override;
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
    /* Base precision request, independent of float-only colorspaces. */
    int precision_prefer_float32;
    int prefer_float32;
    int ormode;
    int transparent_policy;
    int transparent_policy_override;
    int background_policy;
    int background_policy_override;
    int transparent_offset_left;
    int transparent_offset_top;
    unsigned char *accumulation_pixels;
    size_t accumulation_pixels_size;
    unsigned char *accumulation_valid_mask;
    size_t accumulation_valid_mask_size;
    unsigned char *accumulation_mask;
    size_t accumulation_mask_size;
    int accumulation_width;
    int accumulation_height;
    int accumulation_pixelformat;
    /* Interpretation of the emitted RGB retained above, not source/W space.
     * Incompatible output settings cannot reuse this plane's valid bits. */
    int accumulation_color_definition;
    int accumulation_colorspace;
    /* Requested presentation policy is distinct from retained history. */
    int update_policy;
    int progressive_enabled;
    int progressive_override;
    /* Presentation limits are independent of per-step repair work budgets. */
    double progressive_stop_threshold;
    unsigned int progressive_max_passes;
    double progressive_max_seconds;
    int progressive_stop_override;
    int progressive_passes_override;
    int progressive_seconds_override;
    struct sixel_presentation *presentation;
    int sixdelta_enabled;
    unsigned int sixdelta_threshold;
    int residual_enabled;
    int residual_override;
    int sixdelta_metric;
    int sixdelta_metric_override;
    double residual_threshold;
    int residual_threshold_override;
    unsigned int residual_keep_threshold;
    int residual_keep_override;
    unsigned int residual_frames;
    int residual_frames_override;
    /* Tentative observations live here until the output owner commits. */
    sixel_delta_residual_t residual_state;
    /* Runtime ceiling does not mutate the configured maximum or history. */
    unsigned int sixdelta_threshold_limit;
    int sixdelta_refinement_needed;
    size_t sixdelta_painted_pixels;
    int sixdelta_threshold_override;
    int sixdelta_error_mode;
    int sixdelta_error_mode_override;
    /*
     * 6delta retains one plane for the whole displayed surface.  When the
     * caller declares a plane larger than the encoded frame, each frame is
     * placed at sixdelta_origin_* inside it, so a damage rectangle that moves
     * between frames still compares against what the terminal shows at the
     * same screen position.  A zero plane size means "plane == frame".
     */
    int sixdelta_plane_width;
    int sixdelta_plane_height;
    int sixdelta_origin_x;
    int sixdelta_origin_y;
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
    /* Immutable fixed source, separate from frame application state. */
    void *fixed_palette_source;
    int fixed_palette_colorspace;
    int fixed_palette_working;
    int fixed_palette_output;
    int fixed_palette_definition;
    void *diagnostic_dither;
    unsigned short drcs_charset_no;
    int drcs_mmv;
    /* Latest -M export; sRGB8 entries only, independent of wire output. */
    unsigned char *capture_palette;
    size_t capture_palette_size;
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
    /* Output interpretation is separate from frame/working coordinates. */
    int output_color_definition;
    /* Owned endpoint; frame coordinates retain their independent tag. */
    struct sixel_color_def *output_icc;
    int output_gamut;
    struct sixel_gamut *gamut_mapper;
    /* Keep an explicit byte request distinct from automatic selection. */
    int precision_requested;
};

enum sixel_encoder_operation_kind {
    SIXEL_ENCODER_OPERATION_NORMAL,
    SIXEL_ENCODER_OPERATION_REPAIR,
    SIXEL_ENCODER_OPERATION_PLAYBACK
};

/* NORMAL waits; REPAIR returns SIXEL_FALSE when another operation wins.
 * PLAYBACK is private to the producer holding the file-playback reservation.
 * These private boundaries never transfer ownership of caller references. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_operation_enter(sixel_encoder_t *encoder, int kind);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_operation_enter_reason(sixel_encoder_t *encoder, int kind,
                                     int *yield_reason);
SIXEL_INTERNAL_API void
sixel_encoder_operation_leave(sixel_encoder_t *encoder);
SIXEL_INTERNAL_API int
sixel_encoder_operation_pending(sixel_encoder_t *encoder);
SIXEL_INTERNAL_API int
sixel_encoder_operation_admit_output(sixel_encoder_t *encoder);
/* Call under operation ownership, or the file owner's configuration lease.
 * User callbacks are registered for reentrant-mutation rejection. */
SIXEL_INTERNAL_API int
sixel_encoder_is_cancel_requested(sixel_encoder_t const *encoder);


#endif /* LIBSIXEL_ENCODER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
