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

#ifndef LIBSIXEL_DITHER_H
#define LIBSIXEL_DITHER_H

#include <sixel.h>

#include "sixel_atomic.h"
#include "palette.h"

#if !defined(SIXEL_DIFFUSE_INTERFRAME)
# define SIXEL_DIFFUSE_INTERFRAME 0xb
#endif

typedef void (*sixel_dither_pipeline_row_fn)(void *priv, int row_index);

struct sixel_logger;

/*
 * Frame metadata supplied by the encoder. Interframe dithering stages use this
 * context to decide when to keep or reset frame-to-frame state.
 */
typedef struct sixel_dither_frame_context {
    int frame_no;
    int loop_no;
    int multiframe;
    int valid;
} sixel_dither_frame_context_t;

/*
 * Interframe dithering state container. The method_id identifies which
 * interframe strategy owns error_frame so future strategies (for example STBN)
 * can keep state isolated behind a shared lifecycle.
 */
typedef struct sixel_dither_interframe_state {
    void *error_frame;
    size_t error_frame_size;
    int width;
    int height;
    int depth;
    int method_id;
    void *method_private;
    size_t method_private_size;
    unsigned long apply_count;
    unsigned long consume_count;
    unsigned long reset_count;
    unsigned long reset_frame_boundary_count;
    unsigned long reset_size_change_count;
    unsigned long reset_clear_count;
    int last_apply_status;
    int last_apply_consumed;
} sixel_dither_interframe_state_t;

#define SIXEL_DITHER_INTERFRAME_RESET_REASON_NONE 0
#define SIXEL_DITHER_INTERFRAME_RESET_REASON_FRAME_BOUNDARY 1
#define SIXEL_DITHER_INTERFRAME_RESET_REASON_SIZE_CHANGE 2
#define SIXEL_DITHER_INTERFRAME_RESET_REASON_CLEAR 3

/* apply mode for sixel_dither_apply_palette_with_mode() */
#define SIXEL_DITHER_APPLY_PRESERVE_INTERFRAME_STATE 0
#define SIXEL_DITHER_APPLY_CONSUME_INTERFRAME_STATE  1

/* dither context object */
struct sixel_dither {
    sixel_atomic_u32_t ref;         /* reference counter */
    sixel_palette_t *palette;       /* palette definition */
    int reqcolors;                  /* requested colors */
    int force_palette;              /* keep palette size when non-zero */
    int ncolors;                    /* active colors */
    int origcolors;                 /* original colors */
    int optimized;                  /* pixel is 15bpp compressible */
    int optimize_palette;           /* minimize palette size */
    int complexion;                 /* for complexion correction */
    int bodyonly;                   /* do not output palette section if true */
    int method_for_largest;         /* method for finding the largest dimension
                                       for splitting */
    int method_for_rep;             /* method for choosing a color from the box */
    int method_for_diffuse;         /* method for diffusing */
    int method_for_scan;            /* scan order for diffusing */
    int interframe_strategy_override; /* CLI strategy override enable flag */
    int interframe_strategy_token;    /* parsed interframe strategy token */
    int interframe_spatial_diffuse_override; /* CLI spatial diffuse flag */
    int interframe_spatial_diffuse; /* parsed interframe spatial diffusion */
    int interframe_noise_strength_override; /* CLI strength override flag */
    int interframe_noise_strength_u8; /* parsed interframe noise strength */
    int stbn_motion_adapt_override; /* CLI motion adapt override flag */
    int stbn_motion_adapt_enabled;  /* parsed motion adapt toggle */
    int stbn_scene_cut_reset_override; /* CLI scene-cut reset override */
    int stbn_scene_cut_reset_enabled; /* parsed scene-cut reset toggle */
    int stbn_scene_detect_override; /* CLI scene-detect override flag */
    int stbn_scene_detect_enabled;  /* parsed scene-detect toggle */
    int stbn_alpha_guard_override; /* CLI alpha guard override flag */
    int stbn_alpha_guard_enabled;  /* parsed alpha guard toggle */
    int stbn_perceptual_weight_override; /* CLI perceptual weight override */
    int stbn_perceptual_weight_enabled; /* parsed perceptual weight toggle */
    int stbn_fastpath_override;    /* CLI stbn fastpath override flag */
    int stbn_fastpath_enabled;     /* parsed stbn fastpath toggle */
    int bluenoise_strength_override; /* CLI bluenoise strength flag */
    float bluenoise_strength;       /* parsed bluenoise strength */
    int bluenoise_phase_override;   /* CLI bluenoise phase flag */
    int bluenoise_phase_x;          /* parsed bluenoise phase x offset */
    int bluenoise_phase_y;          /* parsed bluenoise phase y offset */
    int bluenoise_seed_override;    /* CLI bluenoise seed flag */
    int bluenoise_seed;             /* parsed bluenoise seed */
    int bluenoise_channel_override; /* CLI bluenoise channel flag */
    int bluenoise_channel_rgb;      /* non-zero for rgb channel mode */
    int bluenoise_size_override;    /* CLI bluenoise size flag */
    int bluenoise_size;             /* parsed bluenoise tile size */
    int bluenoise_gradient_factor_override; /* CLI gradient gamma override */
    float bluenoise_gradient_factor; /* parsed gradient gamma */
    int quality_mode;               /* quality of histogram */
    int requested_quality_mode;     /* original quality mode request */
    int keycolor;                   /* background color */
    unsigned char transparent_bgcolor[3]; /* keycolor compositing backdrop */
    int transparent_bgcolor_valid;  /* non-zero when backdrop is configured */
    int pixelformat;                /* pixelformat for internal processing */
    int prefer_float32;             /* opt-in flag for float32 internals */
    sixel_allocator_t *allocator;   /* allocator */
    int lut_policy;                 /* histogram LUT policy */
    int sixel_reversible;           /* restrict palette to reversible tones */
    int quantize_model;             /* palette solver selector */
    int final_merge_mode;           /* final merge policy */
    sixel_dither_pipeline_row_fn pipeline_row_callback; /* producer hook */
    void *pipeline_row_priv;        /* callback private data */
    sixel_index_t *pipeline_index_buffer; /* externally supplied index buf */
    size_t pipeline_index_size;     /* size of external index buffer */
    int pipeline_index_owned;       /* buffer ownership flag */
    int pipeline_parallel_active;   /* enable overlapped dither bands */
    int pipeline_band_height;       /* band thickness for dither */
    int pipeline_band_overlap;      /* overlap rows for burn-in */
    int pipeline_dither_threads;    /* thread budget for dither */
    int pipeline_pin_threads;       /* pin palette/encode workers */
    int pipeline_image_width;       /* total image columns for masks */
    int pipeline_image_height;      /* total image rows for logging */
    unsigned char const *pipeline_transparent_mask; /* alpha==0 pixels */
    size_t pipeline_transparent_mask_size; /* transparent mask length */
    int pipeline_transparent_keycolor; /* keycolor applied to mask hits */
    unsigned char *bluenoise_gradient_map; /* owned gradient-strength map */
    size_t bluenoise_gradient_map_size; /* gradient map byte length */
    int bluenoise_gradient_width; /* gradient map width */
    int bluenoise_gradient_height; /* gradient map height */
    struct sixel_logger *pipeline_logger; /* parallel log sink */
    sixel_dither_frame_context_t frame_context; /* encoder frame metadata */
    sixel_dither_interframe_state_t interframe_state; /* reserved interframe */
};

#ifdef __cplusplus
extern "C" {
#endif

/* apply palette */
SIXEL_INTERNAL_API sixel_index_t *
sixel_dither_apply_palette(struct sixel_dither /* in */ *dither,
                           unsigned char       /* in */ *pixels,
                           int                 /* in */ width,
                           int                 /* in */ height);

/*
 * Apply palette with explicit interframe-state handling mode. The default API
 * consumes interframe state; capture paths can request preserve mode.
 */
SIXEL_INTERNAL_API sixel_index_t *
sixel_dither_apply_palette_with_mode(struct sixel_dither /* in */ *dither,
                                     unsigned char       /* in */ *pixels,
                                     int                 /* in */ width,
                                     int                 /* in */ height,
                                     int                 /* in */ apply_mode);

/*
 * Attach frame metadata from the encoder to the dither object. Interframe
 * diffusion workers read this metadata to detect loop and timeline boundaries.
 */
SIXEL_INTERNAL_API void
sixel_dither_set_frame_context(sixel_dither_t *dither,
                               int frame_no,
                               int loop_no,
                               int multiframe);

SIXEL_INTERNAL_API void
sixel_dither_clear_frame_context(sixel_dither_t *dither);

SIXEL_INTERNAL_API void
sixel_dither_note_interframe_reset_reason(sixel_dither_t *dither,
                                          int reason);

/*
 * Set or clear a caller-provided transparent mask hint used by the
 * dithering pipeline. The mask pointer is borrowed and must stay valid
 * for the duration of sixel_dither_apply_palette().
 */
SIXEL_INTERNAL_API void
sixel_dither_set_pipeline_transparent_mask_hint(
    sixel_dither_t *dither,
    unsigned char const *transparent_mask,
    size_t transparent_mask_size,
    int keycolor);

void
sixel_dither_clear_pipeline_transparent_mask_hint(
    sixel_dither_t *dither);

SIXEL_INTERNAL_API void
sixel_dither_clear_bluenoise_gradient_map_hint(
    sixel_dither_t *dither);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_set_bluenoise_gradient_map_hint(
    sixel_dither_t *dither,
    unsigned char *gradient_map,
    size_t gradient_map_size,
    int width,
    int height);

/*
 * Configure the alpha preblend backdrop used when transparent keycolor
 * handling keeps alpha-bearing pixels in the palette stage.
 */
void
sixel_dither_set_transparent_bgcolor_hint(
    sixel_dither_t *dither,
    unsigned char const *bgcolor);

void
sixel_dither_clear_transparent_bgcolor_hint(
    sixel_dither_t *dither);


#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_DITHER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
