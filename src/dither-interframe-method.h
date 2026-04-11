/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Common interframe dithering method definitions shared by fixed pipelines.
 */
#ifndef LIBSIXEL_DITHER_INTERFRAME_METHOD_H
#define LIBSIXEL_DITHER_INTERFRAME_METHOD_H

#include <stddef.h>
#include <stdint.h>
#include "dither.h"
#include "dither-interframe-stbn-source.h"

#if !defined(SIXEL_DIFFUSE_INTERFRAME)
# define SIXEL_DIFFUSE_INTERFRAME 0xb
#endif

#define SIXEL_INTERFRAME_METHOD_NONE      0
#define SIXEL_INTERFRAME_METHOD_DIFFUSION 1
#define SIXEL_INTERFRAME_METHOD_STBN      2

#define SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE      0
#define SIXEL_INTERFRAME_STRATEGY_TOKEN_DIFFUSION 1
#define SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_HASH 2
#define SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_MASK 3
#define SIXEL_INTERFRAME_STRATEGY_TOKEN_PMJ       4

#define SIXEL_DITHER_INTERFRAME_DIFFUSION_ENVVAR \
    "SIXEL_DITHER_INTERFRAME_DIFFUSION"
#define SIXEL_DITHER_STBN_SOURCE_ENVVAR \
    "SIXEL_DITHER_STBN_SOURCE"
#define SIXEL_DITHER_STBN_DIFFUSION_ENVVAR \
    "SIXEL_DITHER_STBN_DIFFUSION"
#define SIXEL_DITHER_STBN_STRENGTH_ENVVAR \
    "SIXEL_DITHER_STBN_STRENGTH"
#define SIXEL_DITHER_STBN_MOTION_ADAPT_ENVVAR \
    "SIXEL_DITHER_STBN_MOTION_ADAPT"
#define SIXEL_DITHER_STBN_SCENE_CUT_RESET_ENVVAR \
    "SIXEL_DITHER_STBN_SCENE_CUT_RESET"
#define SIXEL_DITHER_STBN_SCENE_DETECT_ENVVAR \
    "SIXEL_DITHER_STBN_SCENE_DETECT"
#define SIXEL_DITHER_STBN_ALPHA_GUARD_ENVVAR \
    "SIXEL_DITHER_STBN_ALPHA_GUARD"
#define SIXEL_DITHER_STBN_PERCEPTUAL_WEIGHT_ENVVAR \
    "SIXEL_DITHER_STBN_PERCEPTUAL_WEIGHT"
#define SIXEL_DITHER_STBN_FASTPATH_ENVVAR \
    "SIXEL_DITHER_STBN_FASTPATH"

#define SIXEL_INTERFRAME_SPATIAL_DIFFUSE_UNSET (-1)

#define SIXEL_INTERFRAME_VARERR_SCALE_SHIFT 12
#define SIXEL_INTERFRAME_VARERR_SCALE \
    (1 << SIXEL_INTERFRAME_VARERR_SCALE_SHIFT)
#define SIXEL_INTERFRAME_VARERR_ROUND \
    (1 << (SIXEL_INTERFRAME_VARERR_SCALE_SHIFT - 1))
#define SIXEL_INTERFRAME_VARERR_MAX_VALUE \
    (255 * SIXEL_INTERFRAME_VARERR_SCALE)
#define SIXEL_INTERFRAME_SCENE_DETECT_ERROR_THRESHOLD_U8 1

typedef SIXELSTATUS (*sixel_interframe_prepare_frame_fn)(
    sixel_dither_t *dither,
    int width,
    int height,
    int depth,
    int can_update,
    int *enabled,
    int32_t **frame);

typedef void (*sixel_interframe_load_pixel_fn)(
    sixel_dither_t *dither,
    unsigned char const *data,
    size_t base,
    int x,
    int y,
    int depth,
    int32_t const *frame,
    unsigned char *corrected,
    int32_t *accum_scaled);

typedef void (*sixel_interframe_clear_pixel_fn)(
    int32_t *frame,
    size_t base,
    int depth,
    int can_update);

typedef void (*sixel_interframe_store_error_fn)(
    int32_t *frame,
    size_t base,
    int channel,
    int offset,
    int can_update);

typedef struct sixel_interframe_method_ops {
    int method_id;
    sixel_interframe_prepare_frame_fn prepare_frame;
    sixel_interframe_load_pixel_fn load_pixel;
    sixel_interframe_clear_pixel_fn clear_pixel;
    sixel_interframe_store_error_fn store_error;
} sixel_interframe_method_ops_t;

int
sixel_interframe_strategy_token_from_string(char const *value);

int
sixel_interframe_strategy_method_from_token(int strategy_token);

int
sixel_interframe_spatial_diffuse_from_string(char const *value);

int
sixel_interframe_spatial_diffuse_from_env_common(void);

int
sixel_interframe_spatial_diffuse_from_dither_or_env_common(
    sixel_dither_t const *dither);

int
sixel_interframe_strategy_token_from_env_common(void);

int
sixel_interframe_strategy_token_from_dither_or_env_common(
    sixel_dither_t const *dither);

uint8_t
sixel_interframe_stbn_source_id_from_token(int strategy_token);

int
sixel_interframe_method_from_diffuse_and_token(int method_for_diffuse,
                                             int strategy_token);

void
sixel_interframe_release_shared_frame(sixel_dither_t *dither);

SIXELSTATUS
sixel_interframe_prepare_shared_frame(sixel_dither_t *dither,
                                    int width,
                                    int height,
                                    int depth,
                                    int can_update,
                                    int owner_method,
                                    int *enabled,
                                    int32_t **frame);

SIXELSTATUS
sixel_interframe_prepare_method_private(sixel_dither_t *dither,
                                      int owner_method,
                                      int can_update,
                                      size_t state_size,
                                      void **state);

SIXELSTATUS
sixel_interframe_prepare_stbn_state_common(sixel_dither_t *dither,
                                         int can_update,
                                         int strategy_token,
                                         size_t state_size,
                                         void **state);

/*
 * `load_pixel()` calls this on every pixel for STBN paths, so keep it inline
 * in the header to avoid a function-call penalty on hot loops.
 */
static inline void const *
sixel_interframe_get_method_private_const(sixel_dither_t const *dither,
                                        int owner_method,
                                        size_t state_size)
{
    if (dither == NULL || dither->interframe_state.method_private == NULL) {
        return NULL;
    }
    if (dither->interframe_state.method_id != owner_method) {
        return NULL;
    }
    if (dither->interframe_state.method_private_size < state_size) {
        return NULL;
    }

    return dither->interframe_state.method_private;
}

static inline void *
sixel_interframe_get_method_private(sixel_dither_t *dither,
                                  int owner_method,
                                  size_t state_size)
{
    return (void *)sixel_interframe_get_method_private_const(
        (sixel_dither_t const *)dither,
        owner_method,
        state_size);
}

#endif /* LIBSIXEL_DITHER_INTERFRAME_METHOD_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
