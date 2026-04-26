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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */

#include "compat_stub.h"
#include "dither-policy-interframe.h"
#include "dither.h"
#include "dither-common-pipeline.h"
#include "dither-internal.h"
#include "dither-interframe-method.h"
#include "pixelformat.h"
#include "sixel_atomic.h"


static void
sixel_dither_scanline_params_fixed_8bit(int serpentine,
                             int index,
                             int limit,
                             int *start,
                             int *end,
                             int *step,
                             int *direction)
{
    if (serpentine && (index & 1)) {
        *start = limit - 1;
        *end = -1;
        *step = -1;
        *direction = -1;
    } else {
        *start = 0;
        *end = limit;
        *step = 1;
        *direction = 1;
    }
}

typedef sixel_interframe_stbn_state_common_t sixel_interframe_stbn_state_t;

static SIXELSTATUS
sixel_interframe_diffusion_prepare_frame(sixel_dither_t *dither,
                                       int width,
                                       int height,
                                       int depth,
                                       int can_update,
                                       int *enabled,
                                       int32_t **frame);

static void
sixel_interframe_diffusion_load_pixel(
    sixel_dither_t *dither,
    unsigned char const *data,
    size_t base,
    int x,
    int y,
    int depth,
    int32_t const *frame,
    unsigned char corrected[SIXEL_MAX_CHANNELS],
    int32_t accum_scaled[SIXEL_MAX_CHANNELS]);

static void
sixel_interframe_diffusion_clear_pixel(int32_t *frame,
                                     size_t base,
                                     int depth,
                                     int can_update);

static void
sixel_interframe_diffusion_store_error(int32_t *frame,
                                     size_t base,
                                     int channel,
                                     int offset,
                                     int can_update);

static SIXELSTATUS
sixel_interframe_stbn_prepare_frame(sixel_dither_t *dither,
                                  int width,
                                  int height,
                                  int depth,
                                  int can_update,
                                  int *enabled,
                                  int32_t **frame);

static void
sixel_interframe_stbn_load_pixel(
    sixel_dither_t *dither,
    unsigned char const *data,
    size_t base,
    int x,
    int y,
    int depth,
    int32_t const *frame,
    unsigned char corrected[SIXEL_MAX_CHANNELS],
    int32_t accum_scaled[SIXEL_MAX_CHANNELS]);

static void
sixel_interframe_stbn_clear_pixel(int32_t *frame,
                                size_t base,
                                int depth,
                                int can_update);

static void
sixel_interframe_stbn_store_error(int32_t *frame,
                                size_t base,
                                int channel,
                                int offset,
                                int can_update);

static sixel_interframe_method_ops_t const
sixel_interframe_diffusion_ops = {
    SIXEL_INTERFRAME_METHOD_DIFFUSION,
    sixel_interframe_diffusion_prepare_frame,
    sixel_interframe_diffusion_load_pixel,
    sixel_interframe_diffusion_clear_pixel,
    sixel_interframe_diffusion_store_error
};

static sixel_interframe_method_ops_t const
sixel_interframe_stbn_ops = {
    SIXEL_INTERFRAME_METHOD_STBN,
    sixel_interframe_stbn_prepare_frame,
    sixel_interframe_stbn_load_pixel,
    sixel_interframe_stbn_clear_pixel,
    sixel_interframe_stbn_store_error
};

static int
sixel_interframe_method_from_diffuse(sixel_dither_t const *dither,
                                   int method_for_diffuse);

static sixel_interframe_method_ops_t const *
sixel_interframe_method_for_strategy(int interframe_method);

static void
error_diffuse_normal(
    unsigned char /* in */    *data,      /* base address of pixel buffer */
    int           /* in */    pos,        /* address of the destination pixel */
    int           /* in */    depth,      /* color depth in bytes */
    int           /* in */    error,      /* error energy */
    int           /* in */    numerator,  /* numerator of diffusion coefficient */
    int           /* in */    denominator /* denominator of diffusion coefficient */)
{
    int c;

    data += pos * depth;

    c = *data + (error * numerator * 2 / denominator + 1) / 2;
    if (c < 0) {
        c = 0;
    }
    if (c >= 1 << 8) {
        c = (1 << 8) - 1;
    }
    *data = (unsigned char)c;
}

static void
error_diffuse_fast(
    unsigned char /* in */    *data,      /* base address of pixel buffer */
    int           /* in */    pos,        /* address of the destination pixel */
    int           /* in */    depth,      /* color depth in bytes */
    int           /* in */    error,      /* error energy */
    int           /* in */    numerator,  /* numerator of diffusion coefficient */
    int           /* in */    denominator /* denominator of diffusion coefficient */)
{
    int c;

    data += pos * depth;

    c = *data + error * numerator / denominator;
    if (c < 0) {
        c = 0;
    }
    if (c >= 1 << 8) {
        c = (1 << 8) - 1;
    }
    *data = (unsigned char)c;
}

/* error diffusion with precise strategy */
static void
error_diffuse_precise(
    unsigned char /* in */    *data,      /* base address of pixel buffer */
    int           /* in */    pos,        /* address of the destination pixel */
    int           /* in */    depth,      /* color depth in bytes */
    int           /* in */    error,      /* error energy */
    int           /* in */    numerator,  /* numerator of diffusion coefficient */
    int           /* in */    denominator /* denominator of diffusion coefficient */)
{
    int c;

    data += pos * depth;

    c = (int)(*data + error * numerator / (double)denominator + 0.5);
    if (c < 0) {
        c = 0;
    }
    if (c >= 1 << 8) {
        c = (1 << 8) - 1;
    }
    *data = (unsigned char)c;
}

static SIXELSTATUS
sixel_interframe_diffusion_prepare_frame(sixel_dither_t *dither,
                                       int width,
                                       int height,
                                       int depth,
                                       int can_update,
                                       int *enabled,
                                       int32_t **frame)
{
    return sixel_interframe_prepare_shared_frame(dither,
                                               width,
                                               height,
                                               depth,
                                               can_update,
                                               SIXEL_INTERFRAME_METHOD_DIFFUSION,
                                               enabled,
                                               frame);
}

static void
sixel_interframe_diffusion_load_pixel(
    sixel_dither_t *dither,
    unsigned char const *data,
    size_t base,
    int x,
    int y,
    int depth,
    int32_t const *frame,
    unsigned char corrected[SIXEL_MAX_CHANNELS],
    int32_t accum_scaled[SIXEL_MAX_CHANNELS])
{
    int n;
    size_t channel_base;
    int64_t interframe_sum;
    int64_t interframe_clamped;

    (void)dither;
    (void)x;
    (void)y;

    n = 0;
    channel_base = 0U;
    interframe_sum = 0;
    interframe_clamped = 0;

    for (n = 0; n < depth; ++n) {
        channel_base = base + (size_t)n;
        interframe_sum = ((int64_t)data[channel_base]
                        << SIXEL_INTERFRAME_VARERR_SCALE_SHIFT);
        if (frame != NULL) {
            interframe_sum += frame[channel_base];
        }
        if (interframe_sum < INT32_MIN) {
            interframe_sum = INT32_MIN;
        } else if (interframe_sum > INT32_MAX) {
            interframe_sum = INT32_MAX;
        }

        interframe_clamped = interframe_sum;
        if (interframe_clamped < 0) {
            interframe_clamped = 0;
        } else if (interframe_clamped > SIXEL_INTERFRAME_VARERR_MAX_VALUE) {
            interframe_clamped = SIXEL_INTERFRAME_VARERR_MAX_VALUE;
        }
        accum_scaled[n] = (int32_t)interframe_clamped;
        corrected[n] = (unsigned char)((interframe_clamped
                                        + SIXEL_INTERFRAME_VARERR_ROUND)
                                       >> SIXEL_INTERFRAME_VARERR_SCALE_SHIFT);
    }
}

static void
sixel_interframe_diffusion_clear_pixel(int32_t *frame,
                                     size_t base,
                                     int depth,
                                     int can_update)
{
    int n;

    n = 0;
    if (frame == NULL || can_update == 0) {
        return;
    }

    for (n = 0; n < depth; ++n) {
        frame[base + (size_t)n] = 0;
    }
}

static void
sixel_interframe_diffusion_store_error(int32_t *frame,
                                     size_t base,
                                     int channel,
                                     int offset,
                                     int can_update)
{
    int32_t scaled;

    scaled = 0;
    if (frame == NULL || can_update == 0) {
        return;
    }

    /*
     * Multiplication avoids undefined behavior from shifting negative values.
     */
    scaled = (int32_t)(offset * SIXEL_INTERFRAME_VARERR_SCALE);
    frame[base + (size_t)channel] = scaled;
}

static int
sixel_interframe_scene_detect_reset_8bit(int32_t const *frame,
                                         int width,
                                         int height,
                                         int depth)
{
    size_t count;
    size_t i;
    int64_t value64;
    int64_t max_abs;
    int max_u8;

    count = 0U;
    i = 0U;
    value64 = 0;
    max_abs = 0;
    max_u8 = 0;

    if (frame == NULL || width <= 0 || height <= 0 || depth <= 0) {
        return 0;
    }

    count = (size_t)width * (size_t)height * (size_t)depth;
    if (count == 0U) {
        return 0;
    }

    for (i = 0U; i < count; ++i) {
        value64 = frame[i];
        if (value64 < 0) {
            value64 = -value64;
        }
        if (value64 > max_abs) {
            max_abs = value64;
        }
    }

    max_u8 = ((int)max_abs + SIXEL_INTERFRAME_VARERR_ROUND)
        >> SIXEL_INTERFRAME_VARERR_SCALE_SHIFT;

    return max_u8 >= SIXEL_INTERFRAME_SCENE_DETECT_ERROR_THRESHOLD_U8;
}

static SIXELSTATUS
sixel_interframe_stbn_prepare_frame(sixel_dither_t *dither,
                                  int width,
                                  int height,
                                  int depth,
                                  int can_update,
                                  int *enabled,
                                  int32_t **frame)
{
    SIXELSTATUS status;
    sixel_interframe_stbn_state_t *stbn_state;
    int strategy_token;
    size_t frame_bytes;
    int should_reset;
    int scene_detect_hit;

    status = SIXEL_OK;
    stbn_state = NULL;
    strategy_token = SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE;
    frame_bytes = 0U;
    should_reset = 0;
    scene_detect_hit = 0;

    /*
     * STBN strategy reuses the shared interframe state so source backends can
     * change without touching the method call contract.
     */
    status = sixel_interframe_prepare_shared_frame(dither,
                                                 width,
                                                 height,
                                                 depth,
                                                 can_update,
                                                 SIXEL_INTERFRAME_METHOD_STBN,
                                                 enabled,
                                                 frame);
    if (status != SIXEL_OK || *enabled == 0) {
        return status;
    }

    strategy_token = sixel_interframe_strategy_token_from_dither_or_env_common(
        dither);
    status = sixel_interframe_prepare_stbn_state_common(
        dither,
        can_update,
        strategy_token,
        sizeof(sixel_interframe_stbn_state_t),
        (void **)&stbn_state);
    if (status != SIXEL_OK || stbn_state == NULL || can_update == 0) {
        return status;
    }
    if (*frame != NULL
            && dither != NULL
            && dither->frame_context.valid != 0) {
        if (stbn_state->scene_cut_reset_enabled != 0
                && dither->frame_context.frame_no > 0) {
            should_reset = 1;
        } else if (stbn_state->scene_detect_enabled != 0) {
            should_reset = sixel_interframe_scene_detect_reset_8bit(*frame,
                                                                    width,
                                                                    height,
                                                                    depth);
            if (should_reset != 0) {
                scene_detect_hit = 1;
            }
        }
        if (should_reset != 0) {
            frame_bytes = dither->interframe_state.error_frame_size;
            if (frame_bytes > 0U) {
                memset(*frame, 0x00, frame_bytes);
            }
            if (scene_detect_hit != 0) {
                /*
                 * Advance the STBN sequence after a detected cut so the
                 * first frame after reset does not reuse prior phase.
                 */
                stbn_state->sequence_index += 1U;
            }
        }
    }

    return status;
}

static int32_t
sixel_interframe_stbn_bias_scaled_sampled(
    sixel_interframe_stbn_sample_u16_fn sample_fn,
    uint32_t sequence_index,
    int x,
    int y,
    int channel,
    int depth,
    int strength_u8)
{
    uint16_t sample_value;
    int32_t bias_u8;

    sample_value = 0U;
    bias_u8 = 0;

    sample_value = sample_fn(sequence_index, x, y, channel, depth);
    bias_u8 = sixel_interframe_stbn_bias_u8_from_sample_u16_inline_common(
        sample_value,
        strength_u8);
    return bias_u8 * SIXEL_INTERFRAME_VARERR_SCALE;
}

static int32_t
sixel_interframe_stbn_bias_scaled_sampled_row_cached(
    sixel_interframe_stbn_state_t *stbn_state,
    int x,
    int y,
    int channel,
    int depth,
    int strength_u8)
{
    uint16_t sample_value;
    int32_t bias_u8;

    sample_value = 0U;
    bias_u8 = 0;

    sample_value = sixel_interframe_stbn_source_pmj_sample_u16_row_cached_common(
        stbn_state,
        x,
        y,
        channel,
        depth);
    bias_u8 = sixel_interframe_stbn_bias_u8_from_sample_u16_inline_common(
        sample_value,
        strength_u8);
    return bias_u8 * SIXEL_INTERFRAME_VARERR_SCALE;
}

static int32_t
sixel_interframe_stbn_bias_scaled_sampled_tiled(
    sixel_interframe_stbn_state_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth,
    int strength_u8)
{
    uint16_t sample_value;
    int32_t bias_u8;

    sample_value = 0U;
    bias_u8 = 0;

    sample_value = sixel_interframe_stbn_source_pmj_sample_u16_tiled_common(
        stbn_state,
        x,
        y,
        channel,
        depth);
    bias_u8 = sixel_interframe_stbn_bias_u8_from_sample_u16_inline_common(
        sample_value,
        strength_u8);
    return bias_u8 * SIXEL_INTERFRAME_VARERR_SCALE;
}

static int
sixel_interframe_stbn_motion_strength_u8_8bit(int strength_u8,
                                              int32_t const *frame,
                                              size_t base,
                                              int depth)
{
    int n;
    int32_t value32;
    int64_t value64;
    int64_t energy;
    int64_t max_energy;
    int scaled_u8;

    n = 0;
    value32 = 0;
    value64 = 0;
    energy = 0;
    max_energy = 0;
    scaled_u8 = 0;
    if (strength_u8 <= 0 || frame == NULL || depth <= 0) {
        return strength_u8;
    }

    for (n = 0; n < depth; ++n) {
        value32 = frame[base + (size_t)n];
        value64 = value32;
        if (value64 < 0) {
            value64 = -value64;
        }
        if (value64 > SIXEL_INTERFRAME_VARERR_MAX_VALUE) {
            value64 = SIXEL_INTERFRAME_VARERR_MAX_VALUE;
        }
        energy += value64;
    }
    max_energy = (int64_t)depth * (int64_t)SIXEL_INTERFRAME_VARERR_MAX_VALUE;
    if (max_energy <= 0) {
        return strength_u8;
    }

    scaled_u8 = (int)((energy * 255 + (max_energy / 2)) / max_energy);
    if (scaled_u8 < 0) {
        scaled_u8 = 0;
    } else if (scaled_u8 > 255) {
        scaled_u8 = 255;
    }

    return (strength_u8 * scaled_u8 + 127) / 255;
}

static int
sixel_interframe_scene_detect_hit_8bit(int32_t const *frame,
                                       size_t base,
                                       int depth)
{
    int n;
    int64_t value64;
    int64_t energy;
    int64_t threshold;

    n = 0;
    value64 = 0;
    energy = 0;
    threshold = (int64_t)SIXEL_INTERFRAME_VARERR_SCALE;

    if (frame == NULL || depth <= 0) {
        return 0;
    }

    for (n = 0; n < depth; ++n) {
        value64 = frame[base + (size_t)n];
        if (value64 < 0) {
            value64 = -value64;
        }
        energy += value64;
    }

    return energy >= threshold;
}

static int
sixel_interframe_stbn_perceptual_strength_u8_8bit(int strength_u8,
                                                  int channel,
                                                  int depth,
                                                  int enabled)
{
    static int const weights_rgb_u8[3] = { 54, 183, 18 };

    if (enabled == 0 || strength_u8 <= 0) {
        return strength_u8;
    }
    if (depth != 3 || channel < 0 || channel >= 3) {
        return strength_u8;
    }

    return (strength_u8 * weights_rgb_u8[channel] + 127) / 255;
}

static int
sixel_interframe_alpha_guard_hit_8bit(unsigned char const *transparent_mask,
                                     size_t transparent_mask_size,
                                     int width,
                                     int x,
                                     int absolute_y)
{
    int max_rows;
    size_t index;

    max_rows = 0;
    index = 0U;
    if (transparent_mask == NULL || transparent_mask_size == 0U || width <= 0
            || x < 0 || x >= width || absolute_y < 0) {
        return 0;
    }

    max_rows = (int)(transparent_mask_size / (size_t)width);
    if (absolute_y >= max_rows) {
        return 0;
    }

    if (x > 0) {
        index = (size_t)absolute_y * (size_t)width + (size_t)(x - 1);
        if (index < transparent_mask_size && transparent_mask[index] != 0U) {
            return 1;
        }
    }
    if (x + 1 < width) {
        index = (size_t)absolute_y * (size_t)width + (size_t)(x + 1);
        if (index < transparent_mask_size && transparent_mask[index] != 0U) {
            return 1;
        }
    }
    if (absolute_y > 0) {
        index = (size_t)(absolute_y - 1) * (size_t)width + (size_t)x;
        if (index < transparent_mask_size && transparent_mask[index] != 0U) {
            return 1;
        }
    }
    if (absolute_y + 1 < max_rows) {
        index = (size_t)(absolute_y + 1) * (size_t)width + (size_t)x;
        if (index < transparent_mask_size && transparent_mask[index] != 0U) {
            return 1;
        }
    }

    return 0;
}

static void
sixel_interframe_stbn_load_pixel(
    sixel_dither_t *dither,
    unsigned char const *data,
    size_t base,
    int x,
    int y,
    int depth,
    int32_t const *frame,
    unsigned char corrected[SIXEL_MAX_CHANNELS],
    int32_t accum_scaled[SIXEL_MAX_CHANNELS])
{
    int n;
    int32_t bias_scaled;
    int64_t adjusted_scaled;
    sixel_interframe_stbn_state_t *stbn_state;
    sixel_interframe_stbn_sample_u16_fn sample_u16;
    uint32_t sequence_index;
    int use_stbn_bias;
    int use_pmj_row_cached;
    int use_pmj_tiled;
    int stbn_strength_u8;
    int motion_adapt_enabled;
    int scene_detect_enabled;
    int scene_detect_hit;
    int perceptual_weight_enabled;
    int fastpath_enabled;
    int channel_strength_u8;

    n = 0;
    bias_scaled = 0;
    adjusted_scaled = 0;
    sample_u16 = sixel_interframe_stbn_sample_hash_u16_common;
    sequence_index = 0U;
    use_stbn_bias = 0;
    use_pmj_row_cached = 0;
    use_pmj_tiled = 0;
    stbn_strength_u8 = SIXEL_INTERFRAME_STBN_V1_STRENGTH_U8;
    motion_adapt_enabled = 0;
    scene_detect_enabled = 0;
    scene_detect_hit = 0;
    perceptual_weight_enabled = 0;
    fastpath_enabled = 0;
    channel_strength_u8 = 0;
    stbn_state = (sixel_interframe_stbn_state_t *)
        sixel_interframe_get_method_private(
            dither,
            SIXEL_INTERFRAME_METHOD_STBN,
            sizeof(sixel_interframe_stbn_state_t));

    sixel_interframe_diffusion_load_pixel(dither,
                                        data,
                                        base,
                                        x,
                                        y,
                                        depth,
                                        frame,
                                        corrected,
                                        accum_scaled);

    /*
     * 8bit keeps hash-equivalent behavior while mask/pmj apply interframe
     * STBN bias. Cache sampling inputs once per pixel to reduce overhead.
     */
    if (stbn_state != NULL
            && (stbn_state->sample_source_id == SIXEL_INTERFRAME_STBN_SOURCE_MASK
                || stbn_state->sample_source_id
                == SIXEL_INTERFRAME_STBN_SOURCE_PMJ)) {
        use_stbn_bias = 1;
        if (stbn_state->sample_source_id == SIXEL_INTERFRAME_STBN_SOURCE_PMJ) {
            use_pmj_row_cached = 1;
            if (stbn_state->pmj_tile_enabled != 0) {
                use_pmj_tiled = 1;
            }
        }
        sequence_index = stbn_state->sequence_index;
        stbn_strength_u8 = stbn_state->stbn_strength_u8;
        motion_adapt_enabled = stbn_state->motion_adapt_enabled != 0;
        scene_detect_enabled = stbn_state->scene_detect_enabled != 0;
        perceptual_weight_enabled =
            stbn_state->perceptual_weight_enabled != 0;
        fastpath_enabled = stbn_state->fastpath_enabled != 0;
        if (stbn_state->sample_u16 != NULL) {
            sample_u16 = stbn_state->sample_u16;
        }
        if (fastpath_enabled == 0) {
            use_pmj_row_cached = 0;
            use_pmj_tiled = 0;
        }
    }

    if (motion_adapt_enabled != 0) {
        stbn_strength_u8 = sixel_interframe_stbn_motion_strength_u8_8bit(
            stbn_strength_u8,
            frame,
            base,
            depth);
    }
    if (scene_detect_enabled != 0) {
        scene_detect_hit = sixel_interframe_scene_detect_hit_8bit(frame,
                                                                  base,
                                                                  depth);
        if (scene_detect_hit != 0) {
            stbn_strength_u8 = 0;
        }
    }
    if (use_stbn_bias == 0 || stbn_strength_u8 <= 0) {
        return;
    }

    if (use_pmj_tiled != 0) {
        for (n = 0; n < depth; ++n) {
            bias_scaled = sixel_interframe_stbn_bias_scaled_sampled_tiled(
                stbn_state,
                x,
                y,
                n,
                depth,
                sixel_interframe_stbn_perceptual_strength_u8_8bit(
                    stbn_strength_u8,
                    n,
                    depth,
                    perceptual_weight_enabled));
            if (bias_scaled == 0) {
                continue;
            }

            adjusted_scaled = (int64_t)accum_scaled[n] + (int64_t)bias_scaled;
            if (adjusted_scaled < 0) {
                adjusted_scaled = 0;
            } else if (adjusted_scaled > SIXEL_INTERFRAME_VARERR_MAX_VALUE) {
                adjusted_scaled = SIXEL_INTERFRAME_VARERR_MAX_VALUE;
            }

            accum_scaled[n] = (int32_t)adjusted_scaled;
            corrected[n] = (unsigned char)((adjusted_scaled
                                            + SIXEL_INTERFRAME_VARERR_ROUND)
                                           >>
                                           SIXEL_INTERFRAME_VARERR_SCALE_SHIFT);
        }
        return;
    }

    if (use_pmj_row_cached != 0) {
        for (n = 0; n < depth; ++n) {
            bias_scaled = sixel_interframe_stbn_bias_scaled_sampled_row_cached(
                stbn_state,
                x,
                y,
                n,
                depth,
                sixel_interframe_stbn_perceptual_strength_u8_8bit(
                    stbn_strength_u8,
                    n,
                    depth,
                    perceptual_weight_enabled));
            if (bias_scaled == 0) {
                continue;
            }

            adjusted_scaled = (int64_t)accum_scaled[n] + (int64_t)bias_scaled;
            if (adjusted_scaled < 0) {
                adjusted_scaled = 0;
            } else if (adjusted_scaled > SIXEL_INTERFRAME_VARERR_MAX_VALUE) {
                adjusted_scaled = SIXEL_INTERFRAME_VARERR_MAX_VALUE;
            }

            accum_scaled[n] = (int32_t)adjusted_scaled;
            corrected[n] = (unsigned char)((adjusted_scaled
                                            + SIXEL_INTERFRAME_VARERR_ROUND)
                                           >>
                                           SIXEL_INTERFRAME_VARERR_SCALE_SHIFT);
        }
        return;
    }

    for (n = 0; n < depth; ++n) {
        channel_strength_u8 = sixel_interframe_stbn_perceptual_strength_u8_8bit(
            stbn_strength_u8,
            n,
            depth,
            perceptual_weight_enabled);
        if (channel_strength_u8 <= 0) {
            continue;
        }
        bias_scaled = sixel_interframe_stbn_bias_scaled_sampled(
            sample_u16,
            sequence_index,
            x,
            y,
            n,
            depth,
            channel_strength_u8);
        if (bias_scaled == 0) {
            continue;
        }

        adjusted_scaled = (int64_t)accum_scaled[n] + (int64_t)bias_scaled;
        if (adjusted_scaled < 0) {
            adjusted_scaled = 0;
        } else if (adjusted_scaled > SIXEL_INTERFRAME_VARERR_MAX_VALUE) {
            adjusted_scaled = SIXEL_INTERFRAME_VARERR_MAX_VALUE;
        }

        accum_scaled[n] = (int32_t)adjusted_scaled;
        corrected[n] = (unsigned char)((adjusted_scaled
                                        + SIXEL_INTERFRAME_VARERR_ROUND)
                                       >> SIXEL_INTERFRAME_VARERR_SCALE_SHIFT);
    }
}

static void
sixel_interframe_stbn_clear_pixel(int32_t *frame,
                                size_t base,
                                int depth,
                                int can_update)
{
    sixel_interframe_diffusion_clear_pixel(frame,
                                         base,
                                         depth,
                                         can_update);
}

static void
sixel_interframe_stbn_store_error(int32_t *frame,
                                size_t base,
                                int channel,
                                int offset,
                                int can_update)
{
    sixel_interframe_diffusion_store_error(frame,
                                         base,
                                         channel,
                                         offset,
                                         can_update);
}

static int
sixel_interframe_method_from_diffuse(sixel_dither_t const *dither,
                                   int method_for_diffuse)
{
    int token;

    token = sixel_interframe_strategy_token_from_dither_or_env_common(dither);
    return sixel_interframe_method_from_diffuse_and_token(
        method_for_diffuse,
        token);
}

static sixel_interframe_method_ops_t const *
sixel_interframe_method_for_strategy(int interframe_method)
{
    switch (interframe_method) {
    case SIXEL_INTERFRAME_METHOD_DIFFUSION:
        return &sixel_interframe_diffusion_ops;
    case SIXEL_INTERFRAME_METHOD_STBN:
        return &sixel_interframe_stbn_ops;
    default:
        break;
    }

    return NULL;
}

static void diffuse_none(unsigned char *data,
                         int width,
                         int height,
                         int x,
                         int y,
                         int depth,
                         int error,
                         int direction);

static void diffuse_fs(unsigned char *data,
                       int width,
                       int height,
                       int x,
                       int y,
                       int depth,
                       int error,
                       int direction);

static void diffuse_atkinson(unsigned char *data,
                             int width,
                             int height,
                             int x,
                             int y,
                             int depth,
                             int error,
                             int direction);

static void diffuse_jajuni(unsigned char *data,
                           int width,
                           int height,
                           int x,
                           int y,
                           int depth,
                           int error,
                           int direction);

static void diffuse_stucki(unsigned char *data,
                           int width,
                           int height,
                           int x,
                           int y,
                           int depth,
                           int error,
                           int direction);

static void diffuse_burkes(unsigned char *data,
                           int width,
                           int height,
                           int x,
                           int y,
                           int depth,
                           int error,
                           int direction);

static void diffuse_sierra1(unsigned char *data,
                            int width,
                            int height,
                            int x,
                            int y,
                            int depth,
                            int error,
                            int direction);

static void diffuse_sierra2(unsigned char *data,
                            int width,
                            int height,
                            int x,
                            int y,
                            int depth,
                            int error,
                            int direction);

static void diffuse_sierra3(unsigned char *data,
                            int width,
                            int height,
                            int x,
                            int y,
                            int depth,
                            int error,
                            int direction);

static SIXELSTATUS
sixel_dither_apply_fixed_impl(
    sixel_index_t *result,
    unsigned char *data,
    int width,
    int height,
    int band_origin,
    int output_start,
    int depth,
    unsigned char *palette,
    int reqcolor,
    int method_for_scan,
    int optimize_palette,
    sixel_lookup_policy_interface_t const *lookup_policy,
    sixel_dither_lookup_map_fn lookup_map,
    unsigned char new_palette[],
    unsigned short migration_map[],
    int *ncolors,
    int method_for_diffuse,
    float *palette_float,
    float *new_palette_float,
    int float_depth,
    sixel_dither_t *dither)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int serpentine;
    int y;
    int use_interframe;
    int interframe_can_update;
    int interframe_spatial_diffuse;
    int interframe_method;
    int effective_diffuse;
    sixel_interframe_method_ops_t const *interframe_ops;
    int32_t *interframe_error;
    unsigned char corrected[SIXEL_MAX_CHANNELS];
    int32_t accum_scaled[SIXEL_MAX_CHANNELS];
    int start;
    int end;
    int step;
    int direction;
    int x;
    int absolute_y;
    int pos;
    size_t base;
    const unsigned char *source_pixel;
    int color_index;
    int output_index;
    int n;
    int palette_value;
    int offset;
    int float_index;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    int use_transparent_fence;
    int is_transparent;
    size_t absolute_index;
    sixel_interframe_stbn_state_t const *stbn_state;
    int stbn_alpha_guard_enabled;
    int alpha_guard_hit;

    if (depth > SIXEL_MAX_CHANNELS) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    transparent_mask = NULL;
    transparent_mask_size = 0U;
    transparent_keycolor = (-1);
    use_transparent_fence = 0;
    stbn_state = NULL;
    stbn_alpha_guard_enabled = 0;
    alpha_guard_hit = 0;
    if (dither != NULL
            && dither->pipeline_transparent_mask != NULL
            && dither->pipeline_transparent_keycolor >= 0
            && dither->pipeline_transparent_keycolor < SIXEL_PALETTE_MAX) {
        transparent_mask = dither->pipeline_transparent_mask;
        transparent_mask_size = dither->pipeline_transparent_mask_size;
        transparent_keycolor = dither->pipeline_transparent_keycolor;
        use_transparent_fence = 1;
    }

    use_interframe = 0;
    interframe_can_update = 0;
    interframe_spatial_diffuse = SIXEL_DIFFUSE_FS;
    interframe_method = SIXEL_INTERFRAME_METHOD_NONE;
    effective_diffuse = method_for_diffuse;
    interframe_ops = NULL;
    interframe_error = NULL;

    if (method_for_diffuse == SIXEL_DIFFUSE_INTERFRAME) {
        /*
         * Interframe strategy (diffusion/stbn/pmj) and spatial kernel are
         * resolved independently so -d interframe:diffusion=... and
         * -d stbn:diffusion=... share the same kernel selector.
         */
        interframe_spatial_diffuse =
            sixel_interframe_spatial_diffuse_from_dither_or_env_common(
                dither);
        effective_diffuse = interframe_spatial_diffuse;
        interframe_method = sixel_interframe_method_from_diffuse(
            dither,
            method_for_diffuse);
        interframe_ops = sixel_interframe_method_for_strategy(
            interframe_method);
        if (interframe_ops != NULL) {
            use_interframe = 1;
        }
    }
    if (depth != 3) {
        effective_diffuse = SIXEL_DIFFUSE_NONE;
    }

    if (use_interframe) {
        if (dither == NULL || interframe_ops == NULL) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        interframe_can_update = dither->interframe_state.last_apply_consumed;
        status = interframe_ops->prepare_frame(dither,
                                             width,
                                             height,
                                             depth,
                                             interframe_can_update,
                                             &use_interframe,
                                             &interframe_error);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (use_interframe
                && interframe_method == SIXEL_INTERFRAME_METHOD_STBN) {
            stbn_state = (sixel_interframe_stbn_state_t const *)
                sixel_interframe_get_method_private_const(
                    dither,
                    SIXEL_INTERFRAME_METHOD_STBN,
                    sizeof(sixel_interframe_stbn_state_t));
            if (stbn_state != NULL && stbn_state->alpha_guard_enabled != 0) {
                stbn_alpha_guard_enabled = 1;
            }
        }
    }

    serpentine = (method_for_scan == SIXEL_SCAN_SERPENTINE);

    if (optimize_palette) {
        *ncolors = 0;
        memset(new_palette, 0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)depth);
        if (new_palette_float != NULL && float_depth > 0) {
            memset(new_palette_float, 0x00,
                   (size_t)SIXEL_PALETTE_MAX
                       * (size_t)float_depth * sizeof(float));
        }
        memset(migration_map, 0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
    } else {
        *ncolors = reqcolor;
    }

#define SIXEL_DITHER_APPLY_FIXED_8BIT_LOOP(DIFFUSE_FN)                 \
    for (y = 0; y < height; ++y) {                                      \
        absolute_y = band_origin + y;                                   \
        sixel_dither_scanline_params_fixed_8bit(                        \
            serpentine, absolute_y, width,                              \
            &start, &end, &step, &direction);                           \
        for (x = start; x != end; x += step) {                          \
            pos = y * width + x;                                        \
            base = (size_t)pos * (size_t)depth;                         \
            is_transparent = 0;                                         \
            if (use_transparent_fence && absolute_y >= 0) {             \
                absolute_index = (size_t)absolute_y * (size_t)width     \
                    + (size_t)x;                                        \
                if (absolute_index < transparent_mask_size              \
                        && transparent_mask[absolute_index] != 0U) {    \
                    is_transparent = 1;                                 \
                }                                                       \
            }                                                           \
            if (is_transparent) {                                       \
                if (absolute_y >= output_start) {                       \
                    result[pos] = (sixel_index_t)transparent_keycolor;  \
                }                                                       \
                if (use_interframe && interframe_error != NULL          \
                        && interframe_can_update) {                     \
                    interframe_ops->clear_pixel(interframe_error,       \
                                                base,                    \
                                                depth,                   \
                                                interframe_can_update);  \
                }                                                       \
                continue;                                               \
            }                                                           \
            alpha_guard_hit = 0;                                        \
            if (stbn_alpha_guard_enabled != 0 &&                        \
                    use_transparent_fence != 0) {                       \
                alpha_guard_hit = sixel_interframe_alpha_guard_hit_8bit(\
                    transparent_mask,                                   \
                    transparent_mask_size,                              \
                    width,                                              \
                    x,                                                  \
                    absolute_y);                                        \
            } else if (stbn_alpha_guard_enabled != 0                    \
                    && (x == 0 || y == 0                                \
                        || x == width - 1 || y == height - 1)) {        \
                /*                                                       \
                 * When no transparent fence exists, keep alpha_guard   \
                 * from becoming a no-op by applying a conservative     \
                 * border guard.                                        \
                 */                                                      \
                alpha_guard_hit = 1;                                    \
            }                                                           \
            if (use_interframe) {                                       \
                if (alpha_guard_hit != 0                                \
                        && interframe_method ==                         \
                            SIXEL_INTERFRAME_METHOD_STBN) {             \
                    sixel_interframe_diffusion_load_pixel(              \
                        dither,                                         \
                        data,                                           \
                        base,                                           \
                        x,                                              \
                        absolute_y,                                     \
                        depth,                                          \
                        interframe_error,                               \
                        corrected,                                      \
                        accum_scaled);                                  \
                } else {                                                \
                    interframe_ops->load_pixel(                         \
                        dither,                                         \
                        data,                                           \
                        base,                                           \
                        x,                                              \
                        absolute_y,                                     \
                        depth,                                          \
                        interframe_error,                               \
                        corrected,                                      \
                        accum_scaled);                                  \
                }                                                       \
                source_pixel = corrected;                               \
            } else {                                                    \
                source_pixel = data + base;                             \
            }                                                           \
                                                                        \
            color_index = lookup_map(lookup_policy, source_pixel);      \
                                                                        \
            if (optimize_palette) {                                     \
                if (migration_map[color_index] == 0) {                  \
                    output_index = *ncolors;                            \
                    for (n = 0; n < depth; ++n) {                       \
                        new_palette[output_index * depth + n]           \
                            = palette[color_index * depth + n];         \
                    }                                                   \
                    if (palette_float != NULL                           \
                            && new_palette_float != NULL                \
                            && float_depth > 0) {                       \
                        for (float_index = 0;                           \
                                float_index < float_depth;              \
                                ++float_index) {                        \
                            new_palette_float[output_index * float_depth\
                                              + float_index]            \
                                = palette_float[color_index * float_depth\
                                                + float_index];         \
                        }                                               \
                    }                                                   \
                    ++*ncolors;                                         \
                    /*                                                   \
                     * Palette entries are limited to                   \
                     * SIXEL_PALETTE_MAX (256), so storing the count in \
                     * an unsigned short is safe.                       \
                     */                                                  \
                    migration_map[color_index]                          \
                        = (unsigned short)(*ncolors);                   \
                } else {                                                \
                    output_index = migration_map[color_index] - 1;      \
                }                                                       \
            } else {                                                    \
                output_index = color_index;                             \
            }                                                           \
                                                                        \
            if (absolute_y >= output_start) {                           \
                /*                                                       \
                 * Palette indices are bounded by SIXEL_PALETTE_MAX,    \
                 * which fits in sixel_index_t (unsigned char).         \
                 */                                                      \
                result[pos] = (sixel_index_t)output_index;              \
            }                                                           \
                                                                        \
            for (n = 0; n < depth; ++n) {                               \
                if (optimize_palette) {                                 \
                    palette_value = new_palette[output_index * depth    \
                                                + n];                   \
                } else {                                                \
                    palette_value = palette[color_index * depth + n];   \
                }                                                       \
                offset = (int)source_pixel[n] - palette_value;          \
                if (use_interframe) {                                   \
                    interframe_ops->store_error(interframe_error,       \
                                                base,                    \
                                                n,                       \
                                                offset,                  \
                                                interframe_can_update);  \
                }                                                       \
                DIFFUSE_FN(data + n, width, height, x, y,               \
                           depth, offset, direction);                   \
            }                                                           \
        }                                                               \
        if (absolute_y >= output_start) {                               \
            sixel_dither_pipeline_row_notify(dither, absolute_y);       \
        }                                                               \
    }

    switch (effective_diffuse) {
    case SIXEL_DIFFUSE_NONE:
        SIXEL_DITHER_APPLY_FIXED_8BIT_LOOP(diffuse_none);
        break;
    case SIXEL_DIFFUSE_ATKINSON:
        SIXEL_DITHER_APPLY_FIXED_8BIT_LOOP(diffuse_atkinson);
        break;
    case SIXEL_DIFFUSE_JAJUNI:
        SIXEL_DITHER_APPLY_FIXED_8BIT_LOOP(diffuse_jajuni);
        break;
    case SIXEL_DIFFUSE_STUCKI:
        SIXEL_DITHER_APPLY_FIXED_8BIT_LOOP(diffuse_stucki);
        break;
    case SIXEL_DIFFUSE_BURKES:
        SIXEL_DITHER_APPLY_FIXED_8BIT_LOOP(diffuse_burkes);
        break;
    case SIXEL_DIFFUSE_SIERRA1:
        SIXEL_DITHER_APPLY_FIXED_8BIT_LOOP(diffuse_sierra1);
        break;
    case SIXEL_DIFFUSE_SIERRA2:
        SIXEL_DITHER_APPLY_FIXED_8BIT_LOOP(diffuse_sierra2);
        break;
    case SIXEL_DIFFUSE_SIERRA3:
        SIXEL_DITHER_APPLY_FIXED_8BIT_LOOP(diffuse_sierra3);
        break;
    case SIXEL_DIFFUSE_INTERFRAME:
        SIXEL_DITHER_APPLY_FIXED_8BIT_LOOP(diffuse_fs);
        break;
    case SIXEL_DIFFUSE_FS:
        SIXEL_DITHER_APPLY_FIXED_8BIT_LOOP(diffuse_fs);
        break;
    default:
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
#undef SIXEL_DITHER_APPLY_FIXED_8BIT_LOOP

    (void)effective_diffuse;
    (void)interframe_spatial_diffuse;

    if (optimize_palette) {
        memcpy(palette, new_palette, (size_t)(*ncolors * depth));
        if (palette_float != NULL
                && new_palette_float != NULL
                && float_depth > 0) {
            memcpy(palette_float,
                   new_palette_float,
                   (size_t)(*ncolors * float_depth) * sizeof(float));
        }
    }

    status = SIXEL_OK;

end:
    return status;
}

static SIXELSTATUS
sixel_dither_apply_fixed_8bit_with_mode(sixel_dither_t *dither,
                                        sixel_dither_context_t *context,
                                        int method_for_diffuse)
{
    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->pixels == NULL || context->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->result == NULL || context->new_palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->migration_map == NULL || context->ncolors == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->lookup_policy == NULL || context->lookup_map == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return sixel_dither_apply_fixed_impl(context->result,
                                         context->pixels,
                                         context->width,
                                         context->height,
                                         context->band_origin,
                                         context->output_start,
                                         context->depth,
                                         context->palette,
                                         context->reqcolor,
                                         context->method_for_scan,
                                         context->optimize_palette,
                                         context->lookup_policy,
                                         context->lookup_map,
                                         context->new_palette,
                                         context->migration_map,
                                         context->ncolors,
                                         method_for_diffuse,
                                         context->palette_float,
                                         context->new_palette_float,
                                         context->float_depth,
                                         dither);
}










static SIXELSTATUS
sixel_dither_apply_interframe_8bit(sixel_dither_t *dither,
                                   sixel_dither_context_t *context)
{
    return sixel_dither_apply_fixed_8bit_with_mode(
        dither, context, SIXEL_DIFFUSE_INTERFRAME);
}

static void
diffuse_none(unsigned char *data, int width, int height,
             int x, int y, int depth, int error, int direction)
{
    /* unused */ (void) data;
    /* unused */ (void) width;
    /* unused */ (void) height;
    /* unused */ (void) x;
    /* unused */ (void) y;
    /* unused */ (void) depth;
    /* unused */ (void) error;
    /* unused */ (void) direction;
}


static void
diffuse_fs(unsigned char *data, int width, int height,
           int x, int y, int depth, int error, int direction)
{
    /* Floyd Steinberg Method
     *          curr    7/16
     *  3/16    5/16    1/16
     */
    int pos;
    int forward;

    pos = y * width + x;
    forward = direction >= 0;

    if (forward) {
        if (x < width - 1) {
            error_diffuse_normal(data, pos + 1, depth, error, 7, 16);
        }
        if (y < height - 1) {
            if (x > 0) {
                error_diffuse_normal(data,
                                     pos + width - 1,
                                     depth, error, 3, 16);
            }
            error_diffuse_normal(data,
                                 pos + width,
                                 depth, error, 5, 16);
            if (x < width - 1) {
                error_diffuse_normal(data,
                                     pos + width + 1,
                                     depth, error, 1, 16);
            }
        }
    } else {
        if (x > 0) {
            error_diffuse_normal(data, pos - 1, depth, error, 7, 16);
        }
        if (y < height - 1) {
            if (x < width - 1) {
                error_diffuse_normal(data,
                                     pos + width + 1,
                                     depth, error, 3, 16);
            }
            error_diffuse_normal(data,
                                 pos + width,
                                 depth, error, 5, 16);
            if (x > 0) {
                error_diffuse_normal(data,
                                     pos + width - 1,
                                     depth, error, 1, 16);
            }
        }
    }
}


static void
diffuse_atkinson(unsigned char *data, int width, int height,
                 int x, int y, int depth, int error, int direction)
{
    /* Atkinson's Method
     *          curr    1/8    1/8
     *   1/8     1/8    1/8
     *           1/8
     */
    int pos;
    int sign;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    if (x + sign >= 0 && x + sign < width) {
        error_diffuse_fast(data, pos + sign, depth, error, 1, 8);
    }
    if (x + sign * 2 >= 0 && x + sign * 2 < width) {
        error_diffuse_fast(data, pos + sign * 2, depth, error, 1, 8);
    }
    if (y < height - 1) {
        int row;

        row = pos + width;
        if (x - sign >= 0 && x - sign < width) {
            error_diffuse_fast(data,
                               row + (-sign),
                               depth, error, 1, 8);
        }
        error_diffuse_fast(data, row, depth, error, 1, 8);
        if (x + sign >= 0 && x + sign < width) {
            error_diffuse_fast(data,
                               row + sign,
                               depth, error, 1, 8);
        }
    }
    if (y < height - 2) {
        error_diffuse_fast(data, pos + width * 2, depth, error, 1, 8);
    }
}


static void
diffuse_jajuni(unsigned char *data, int width, int height,
               int x, int y, int depth, int error, int direction)
{
    /* Jarvis, Judice & Ninke Method
     *                  curr    7/48    5/48
     *  3/48    5/48    7/48    5/48    3/48
     *  1/48    3/48    5/48    3/48    1/48
     */
    int pos;
    int sign;
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_weights[] = { 7, 5 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_weights[] = { 3, 5, 7, 5, 3 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_weights[] = { 1, 3, 5, 3, 1 };
    int i;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
        int neighbor;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_precise(data,
                              pos + (neighbor - x),
                              depth, error,
                              row0_weights[i], 48);
    }
    if (y < height - 1) {
        int row;

        row = pos + width;
        for (i = 0; i < 5; ++i) {
            int neighbor;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row1_weights[i], 48);
        }
    }
    if (y < height - 2) {
        int row;

        row = pos + width * 2;
        for (i = 0; i < 5; ++i) {
            int neighbor;

            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row2_weights[i], 48);
        }
    }
}


static void
diffuse_stucki(unsigned char *data, int width, int height,
               int x, int y, int depth, int error, int direction)
{
    /* Stucki's Method
     *                  curr    8/48    4/48
     *  2/48    4/48    8/48    4/48    2/48
     *  1/48    2/48    4/48    2/48    1/48
     */
    int pos;
    int sign;
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 6, 12 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 24, 12, 6, 12, 24 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_num[] = { 1, 1, 1, 1, 1 };
    static const int row2_den[] = { 48, 24, 12, 24, 48 };
    int i;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
        int neighbor;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_precise(data,
                              pos + (neighbor - x),
                              depth, error,
                              row0_num[i], row0_den[i]);
    }
    if (y < height - 1) {
        int row;

        row = pos + width;
        for (i = 0; i < 5; ++i) {
            int neighbor;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row1_num[i], row1_den[i]);
        }
    }
    if (y < height - 2) {
        int row;

        row = pos + width * 2;
        for (i = 0; i < 5; ++i) {
            int neighbor;

            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row2_num[i], row2_den[i]);
        }
    }
}


static void
diffuse_burkes(unsigned char *data, int width, int height,
               int x, int y, int depth, int error, int direction)
{
    /* Burkes' Method
     *                  curr    4/16    2/16
     *  1/16    2/16    4/16    2/16    1/16
     */
    int pos;
    int sign;
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 4, 8 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 16, 8, 4, 8, 16 };
    int i;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
        int neighbor;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_normal(data,
                             pos + (neighbor - x),
                             depth, error,
                             row0_num[i], row0_den[i]);
    }
    if (y < height - 1) {
        int row;

        row = pos + width;
        for (i = 0; i < 5; ++i) {
            int neighbor;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_normal(data,
                                 row + (neighbor - x),
                                 depth, error,
                                 row1_num[i], row1_den[i]);
        }
    }
}

static void
diffuse_sierra1(unsigned char *data, int width, int height,
                int x, int y, int depth, int error, int direction)
{
    /* Sierra Lite Method
     *          curr    2/4
     *  1/4     1/4
     */
    static const int row0_offsets[] = { 1 };
    static const int row0_num[] = { 1 };
    static const int row0_den[] = { 2 };
    static const int row1_offsets[] = { -1, 0 };
    static const int row1_num[] = { 1, 1 };
    static const int row1_den[] = { 4, 4 };
    int pos;
    int sign;
    int i;
    int neighbor;
    int row;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 1; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_normal(data,
                             pos + (neighbor - x),
                             depth, error,
                             row0_num[i], row0_den[i]);
    }
    if (y < height - 1) {
        row = pos + width;
        for (i = 0; i < 2; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_normal(data,
                                 row + (neighbor - x),
                                 depth, error,
                                 row1_num[i], row1_den[i]);
        }
    }
}


static void
diffuse_sierra2(unsigned char *data, int width, int height,
                int x, int y, int depth, int error, int direction)
{
    /* Sierra Two-row Method
     *                  curr    4/32    3/32
     *  1/32    2/32    3/32    2/32    1/32
     *                  2/32    3/32    2/32
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 4, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 2, 3, 2, 1 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
    int pos;
    int sign;
    int i;
    int neighbor;
    int row;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_precise(data,
                              pos + (neighbor - x),
                              depth, error,
                              row0_num[i], row0_den[i]);
    }
    if (y < height - 1) {
        row = pos + width;
        for (i = 0; i < 5; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row1_num[i], row1_den[i]);
        }
    }
    if (y < height - 2) {
        row = pos + width * 2;
        for (i = 0; i < 3; ++i) {
            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row2_num[i], row2_den[i]);
        }
    }
}


static void
diffuse_sierra3(unsigned char *data, int width, int height,
                int x, int y, int depth, int error, int direction)
{
    /* Sierra-3 Method
     *                  curr    5/32    3/32
     *  2/32    4/32    5/32    4/32    2/32
     *                  2/32    3/32    2/32
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 5, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 2, 4, 5, 4, 2 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
    int pos;
    int sign;
    int i;
    int neighbor;
    int row;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_precise(data,
                              pos + (neighbor - x),
                              depth, error,
                              row0_num[i], row0_den[i]);
    }
    if (y < height - 1) {
        row = pos + width;
        for (i = 0; i < 5; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row1_num[i], row1_den[i]);
        }
    }
    if (y < height - 2) {
        row = pos + width * 2;
        for (i = 0; i < 3; ++i) {
            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row2_num[i], row2_den[i]);
        }
    }
}

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_DITHER_FIXED_8BIT_UNUSED __attribute__((used))
#else
# define SIXEL_DITHER_FIXED_8BIT_UNUSED
#endif

/*
 * Keep helper symbols referenced in single-policy translation units so
 * -Wunused-function does not fire when compile-time selection removes
 * alternate code paths.
 */
static void (* const SIXEL_DITHER_FIXED_8BIT_UNUSED
sixel_dither_fixed_8bit_keep_diffuse_fns[])(
    unsigned char *,
    int,
    int,
    int,
    int,
    int,
    int,
    int) = {
    diffuse_none,
    diffuse_fs,
    diffuse_atkinson,
    diffuse_jajuni,
    diffuse_stucki,
    diffuse_burkes,
    diffuse_sierra1,
    diffuse_sierra2,
    diffuse_sierra3
};

static int (* const SIXEL_DITHER_FIXED_8BIT_UNUSED
sixel_dither_fixed_8bit_keep_interframe_from_diffuse[])(
    sixel_dither_t const *,
    int) = {
    sixel_interframe_method_from_diffuse
};

static sixel_interframe_method_ops_t const *(
    * const SIXEL_DITHER_FIXED_8BIT_UNUSED
    sixel_dither_fixed_8bit_keep_interframe_strategy[])(int) = {
    sixel_interframe_method_for_strategy
};

#undef SIXEL_DITHER_FIXED_8BIT_UNUSED

typedef SIXELSTATUS (*sixel_interframe_prepare_frame_float32_fn)(
    sixel_dither_t *dither,
    int width,
    int height,
    int depth,
    int can_update,
    int *enabled,
    int32_t **frame);

typedef void (*sixel_interframe_load_pixel_float32_fn)(
    sixel_dither_t *dither,
    float const *source_pixel,
    size_t base,
    int x,
    int y,
    int depth,
    int pixelformat,
    int32_t const *frame,
    float working_float[SIXEL_MAX_CHANNELS],
    unsigned char corrected[SIXEL_MAX_CHANNELS]);

typedef void (*sixel_interframe_clear_pixel_float32_fn)(
    int32_t *frame,
    size_t base,
    int depth,
    int can_update);

typedef void (*sixel_interframe_store_error_float32_fn)(
    int32_t *frame,
    size_t base,
    int channel,
    int offset,
    int can_update);

typedef struct sixel_interframe_method_float32_ops {
    int method_id;
    sixel_interframe_prepare_frame_float32_fn prepare_frame;
    sixel_interframe_load_pixel_float32_fn load_pixel;
    sixel_interframe_clear_pixel_float32_fn clear_pixel;
    sixel_interframe_store_error_float32_fn store_error;
} sixel_interframe_method_float32_ops_t;

typedef sixel_interframe_stbn_state_common_t
    sixel_interframe_stbn_state_float32_t;

static void
error_diffuse_float(float *data,
                    int pos,
                    int depth,
                    float error,
                    int numerator,
                    int denominator,
                    int pixelformat,
                    int channel_index)
{
    float *channel;
    float delta;

    channel = data + ((size_t)pos * (size_t)depth);
    delta = error * ((float)numerator / (float)denominator);
    *channel += delta;
    *channel = sixel_pixelformat_float_channel_clamp(pixelformat,
                                                     channel_index,
                                                     *channel);
}

static void
sixel_dither_scanline_params_fixed_float32(int serpentine,
                             int index,
                             int limit,
                             int *start,
                             int *end,
                             int *step,
                             int *direction)
{
    if (serpentine && (index & 1)) {
        *start = limit - 1;
        *end = -1;
        *step = -1;
        *direction = -1;
    } else {
        *start = 0;
        *end = limit;
        *step = 1;
        *direction = 1;
    }
}

static void
diffuse_none_float(float *data,
                   int width,
                   int height,
                   int x,
                   int y,
                   int depth,
                   float error,
                   int direction,
                   int pixelformat,
                   int channel_index)
{
    (void)data;
    (void)width;
    (void)height;
    (void)x;
    (void)y;
    (void)depth;
    (void)error;
    (void)direction;
    (void)pixelformat;
    (void)channel_index;
}

static void
diffuse_fs_float(float *data,
                 int width,
                 int height,
                 int x,
                 int y,
                 int depth,
                 float error,
                 int direction,
                 int pixelformat,
                 int channel_index)
{
    int pos;
    int forward;

    pos = y * width + x;
    forward = direction >= 0;

    if (forward) {
        if (x < width - 1) {
            error_diffuse_float(data,
                                pos + 1,
                                depth,
                                error,
                                7,
                                16,
                                pixelformat,
                                channel_index);
        }
        if (y < height - 1) {
            if (x > 0) {
                error_diffuse_float(data,
                                    pos + width - 1,
                                    depth,
                                    error,
                                    3,
                                    16,
                                    pixelformat,
                                    channel_index);
            }
            error_diffuse_float(data,
                                pos + width,
                                depth,
                                error,
                                5,
                                16,
                                pixelformat,
                                channel_index);
            if (x < width - 1) {
                error_diffuse_float(data,
                                    pos + width + 1,
                                    depth,
                                    error,
                                    1,
                                    16,
                                    pixelformat,
                                    channel_index);
            }
        }
    } else {
        if (x > 0) {
            error_diffuse_float(data,
                                pos - 1,
                                depth,
                                error,
                                7,
                                16,
                                pixelformat,
                                channel_index);
        }
        if (y < height - 1) {
            if (x < width - 1) {
                error_diffuse_float(data,
                                    pos + width + 1,
                                    depth,
                                    error,
                                    3,
                                    16,
                                    pixelformat,
                                    channel_index);
            }
            error_diffuse_float(data,
                                pos + width,
                                depth,
                                error,
                                5,
                                16,
                                pixelformat,
                                channel_index);
            if (x > 0) {
                error_diffuse_float(data,
                                    pos + width - 1,
                                    depth,
                                    error,
                                    1,
                                    16,
                                    pixelformat,
                                    channel_index);
            }
        }
    }
}

/*
 * Atkinson's kernel spreads the error within a 3x3 neighborhood using
 * symmetric 1/8 weights.  The float variant mirrors the integer version
 * but keeps the higher precision samples intact.
 */
static void
diffuse_atkinson_float(float *data,
                       int width,
                       int height,
                       int x,
                       int y,
                       int depth,
                       float error,
                       int direction,
                       int pixelformat,
                       int channel_index)
{
    int pos;
    int sign;
    int row;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    if (x + sign >= 0 && x + sign < width) {
        error_diffuse_float(data,
                            pos + sign,
                            depth,
                            error,
                            1,
                            8,
                            pixelformat,
                            channel_index);
    }
    if (x + sign * 2 >= 0 && x + sign * 2 < width) {
        error_diffuse_float(data,
                            pos + sign * 2,
                            depth,
                            error,
                            1,
                            8,
                            pixelformat,
                            channel_index);
    }
    if (y < height - 1) {
        row = pos + width;
        if (x - sign >= 0 && x - sign < width) {
            error_diffuse_float(data,
                                row - sign,
                                depth,
                                error,
                                1,
                                8,
                                pixelformat,
                                channel_index);
        }
        error_diffuse_float(data,
                            row,
                            depth,
                            error,
                            1,
                            8,
                            pixelformat,
                            channel_index);
        if (x + sign >= 0 && x + sign < width) {
            error_diffuse_float(data,
                                row + sign,
                                depth,
                                error,
                                1,
                                8,
                                pixelformat,
                                channel_index);
        }
    }
    if (y < height - 2) {
        error_diffuse_float(data,
                            pos + width * 2,
                            depth,
                            error,
                            1,
                            8,
                            pixelformat,
                            channel_index);
    }
}

/*
 * Shared helper that applies a row of diffusion weights to neighbors on the
 * current or subsequent scanlines.  Each caller provides the offset table and
 * numerator/denominator pairs so the classic kernels can be described using a
 * compact table instead of open-coded loops.
 */
static void
diffuse_weighted_row(float *data,
                     int pos,
                     int depth,
                     float error,
                     int direction,
                     int pixelformat,
                     int channel_index,
                     int x,
                     int width,
                     int row_offset,
                     int const *offsets,
                     int const *numerators,
                     int const *denominators,
                     int count)
{
    int i;
    int neighbor;
    int row_base;
    int sign;

    sign = direction >= 0 ? 1 : -1;
    row_base = pos + row_offset;
    for (i = 0; i < count; ++i) {
        neighbor = x + sign * offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_float(data,
                            row_base + (neighbor - x),
                            depth,
                            error,
                            numerators[i],
                            denominators[i],
                            pixelformat,
                            channel_index);
    }
}

/*
 * Jarvis, Judice, and Ninke kernel using the canonical 5x3 mask.  Three rows
 * of weights are applied with consistent 1/48 denominators to preserve the
 * reference diffusion matrix.
 */
static void
diffuse_jajuni_float(float *data,
                     int width,
                     int height,
                     int x,
                     int y,
                     int depth,
                     float error,
                     int direction,
                     int pixelformat,
                     int channel_index)
{
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 7, 5 };
    static const int row0_den[] = { 48, 48 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 3, 5, 7, 5, 3 };
    static const int row1_den[] = { 48, 48, 48, 48, 48 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_num[] = { 1, 3, 5, 3, 1 };
    static const int row2_den[] = { 48, 48, 48, 48, 48 };
    int pos;

    pos = y * width + x;
    diffuse_weighted_row(data,
                         pos,
                         depth,
                         error,
                         direction,
                         pixelformat,
                         channel_index,
                         x,
                         width,
                         0,
                         row0_offsets,
                         row0_num,
                         row0_den,
                         2);
    if (y < height - 1) {
        diffuse_weighted_row(data,
                             pos,
                             depth,
                             error,
                             direction,
                             pixelformat,
                             channel_index,
                             x,
                             width,
                             width,
                             row1_offsets,
                             row1_num,
                             row1_den,
                             5);
    }
    if (y < height - 2) {
        diffuse_weighted_row(data,
                             pos,
                             depth,
                             error,
                             direction,
                             pixelformat,
                             channel_index,
                             x,
                             width,
                             width * 2,
                             row2_offsets,
                             row2_num,
                             row2_den,
                             5);
    }
}

/*
 * Stucki's method spreads the error across a 5x3 neighborhood with larger
 * emphasis on closer pixels.  The numerators/denominators match the classic
 * 8/48, 4/48, and related fractions from the integer backend.
 */
static void
diffuse_stucki_float(float *data,
                     int width,
                     int height,
                     int x,
                     int y,
                     int depth,
                     float error,
                     int direction,
                     int pixelformat,
                     int channel_index)
{
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 6, 12 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 24, 12, 6, 12, 24 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_num[] = { 1, 1, 1, 1, 1 };
    static const int row2_den[] = { 48, 24, 12, 24, 48 };
    int pos;

    pos = y * width + x;
    diffuse_weighted_row(data,
                         pos,
                         depth,
                         error,
                         direction,
                         pixelformat,
                         channel_index,
                         x,
                         width,
                         0,
                         row0_offsets,
                         row0_num,
                         row0_den,
                         2);
    if (y < height - 1) {
        diffuse_weighted_row(data,
                             pos,
                             depth,
                             error,
                             direction,
                             pixelformat,
                             channel_index,
                             x,
                             width,
                             width,
                             row1_offsets,
                             row1_num,
                             row1_den,
                             5);
    }
    if (y < height - 2) {
        diffuse_weighted_row(data,
                             pos,
                             depth,
                             error,
                             direction,
                             pixelformat,
                             channel_index,
                             x,
                             width,
                             width * 2,
                             row2_offsets,
                             row2_num,
                             row2_den,
                             5);
    }
}

/*
 * Burkes' kernel limits the spread to two rows to reduce directional artifacts
 * while keeping the symmetric 1/16-4/16 pattern.
 */
static void
diffuse_burkes_float(float *data,
                     int width,
                     int height,
                     int x,
                     int y,
                     int depth,
                     float error,
                     int direction,
                     int pixelformat,
                     int channel_index)
{
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 4, 8 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 16, 8, 4, 8, 16 };
    int pos;

    pos = y * width + x;
    diffuse_weighted_row(data,
                         pos,
                         depth,
                         error,
                         direction,
                         pixelformat,
                         channel_index,
                         x,
                         width,
                         0,
                         row0_offsets,
                         row0_num,
                         row0_den,
                         2);
    if (y < height - 1) {
        diffuse_weighted_row(data,
                             pos,
                             depth,
                             error,
                             direction,
                             pixelformat,
                             channel_index,
                             x,
                             width,
                             width,
                             row1_offsets,
                             row1_num,
                             row1_den,
                             5);
    }
}

/*
 * Sierra Lite (Sierra1) uses a compact 2x2 mask to reduce ringing while
 * keeping serpentine traversal stable.
 */
static void
diffuse_sierra1_float(float *data,
                      int width,
                      int height,
                      int x,
                      int y,
                      int depth,
                      float error,
                      int direction,
                      int pixelformat,
                      int channel_index)
{
    static const int row0_offsets[] = { 1 };
    static const int row0_num[] = { 1 };
    static const int row0_den[] = { 2 };
    static const int row1_offsets[] = { -1, 0 };
    static const int row1_num[] = { 1, 1 };
    static const int row1_den[] = { 4, 4 };
    int pos;

    pos = y * width + x;
    diffuse_weighted_row(data,
                         pos,
                         depth,
                         error,
                         direction,
                         pixelformat,
                         channel_index,
                         x,
                         width,
                         0,
                         row0_offsets,
                         row0_num,
                         row0_den,
                         1);
    if (y < height - 1) {
        diffuse_weighted_row(data,
                             pos,
                             depth,
                             error,
                             direction,
                             pixelformat,
                             channel_index,
                             x,
                             width,
                             width,
                             row1_offsets,
                             row1_num,
                             row1_den,
                             2);
    }
}

/*
 * Sierra Two-row keeps the full 5x3 footprint but halves the lower row weights
 * relative to Sierra-3, matching the 32-denominator formulation.
 */
static void
diffuse_sierra2_float(float *data,
                      int width,
                      int height,
                      int x,
                      int y,
                      int depth,
                      float error,
                      int direction,
                      int pixelformat,
                      int channel_index)
{
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 4, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 2, 3, 2, 1 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
    int pos;

    pos = y * width + x;
    diffuse_weighted_row(data,
                         pos,
                         depth,
                         error,
                         direction,
                         pixelformat,
                         channel_index,
                         x,
                         width,
                         0,
                         row0_offsets,
                         row0_num,
                         row0_den,
                         2);
    if (y < height - 1) {
        diffuse_weighted_row(data,
                             pos,
                             depth,
                             error,
                             direction,
                             pixelformat,
                             channel_index,
                             x,
                             width,
                             width,
                             row1_offsets,
                             row1_num,
                             row1_den,
                             5);
    }
    if (y < height - 2) {
        diffuse_weighted_row(data,
                             pos,
                             depth,
                             error,
                             direction,
                             pixelformat,
                             channel_index,
                             x,
                             width,
                             width * 2,
                             row2_offsets,
                             row2_num,
                             row2_den,
                             3);
    }
}

/*
 * Sierra-3 restores the heavier middle-row contributions (5/32) that
 * characterize the original kernel.
 */
static void
diffuse_sierra3_float(float *data,
                      int width,
                      int height,
                      int x,
                      int y,
                      int depth,
                      float error,
                      int direction,
                      int pixelformat,
                      int channel_index)
{
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 5, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 2, 4, 5, 4, 2 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
    int pos;

    pos = y * width + x;
    diffuse_weighted_row(data,
                         pos,
                         depth,
                         error,
                         direction,
                         pixelformat,
                         channel_index,
                         x,
                         width,
                         0,
                         row0_offsets,
                         row0_num,
                         row0_den,
                         2);
    if (y < height - 1) {
        diffuse_weighted_row(data,
                             pos,
                             depth,
                             error,
                             direction,
                             pixelformat,
                             channel_index,
                             x,
                             width,
                             width,
                             row1_offsets,
                             row1_num,
                             row1_den,
                             5);
    }
    if (y < height - 2) {
        diffuse_weighted_row(data,
                             pos,
                             depth,
                             error,
                             direction,
                             pixelformat,
                             channel_index,
                             x,
                             width,
                             width * 2,
                             row2_offsets,
                             row2_num,
                             row2_den,
                             3);
    }
}

static void
sixel_interframe_clear_pixel_float32(int32_t *frame,
                                   size_t base,
                                   int depth,
                                   int can_update)
{
    int n;

    n = 0;
    if (frame == NULL || can_update == 0) {
        return;
    }
    for (n = 0; n < depth; ++n) {
        frame[base + (size_t)n] = 0;
    }
}

static void
sixel_interframe_store_error_float32(int32_t *frame,
                                   size_t base,
                                   int channel,
                                   int offset,
                                   int can_update)
{
    int32_t scaled;

    scaled = 0;
    if (frame == NULL || can_update == 0) {
        return;
    }

    /*
     * Multiplication keeps signed offsets defined on every compiler.
     */
    scaled = (int32_t)(offset * SIXEL_INTERFRAME_VARERR_SCALE);
    frame[base + (size_t)channel] = scaled;
}

static int
sixel_interframe_scene_detect_reset_float32(int32_t const *frame,
                                            int width,
                                            int height,
                                            int depth)
{
    size_t count;
    size_t i;
    int64_t value64;
    int64_t max_abs;
    int max_u8;

    count = 0U;
    i = 0U;
    value64 = 0;
    max_abs = 0;
    max_u8 = 0;

    if (frame == NULL || width <= 0 || height <= 0 || depth <= 0) {
        return 0;
    }

    count = (size_t)width * (size_t)height * (size_t)depth;
    if (count == 0U) {
        return 0;
    }

    for (i = 0U; i < count; ++i) {
        value64 = frame[i];
        if (value64 < 0) {
            value64 = -value64;
        }
        if (value64 > max_abs) {
            max_abs = value64;
        }
    }

    max_u8 = ((int)max_abs + SIXEL_INTERFRAME_VARERR_ROUND)
        >> SIXEL_INTERFRAME_VARERR_SCALE_SHIFT;

    return max_u8 >= SIXEL_INTERFRAME_SCENE_DETECT_ERROR_THRESHOLD_U8;
}

static SIXELSTATUS
sixel_interframe_diffusion_prepare_frame_float32(sixel_dither_t *dither,
                                               int width,
                                               int height,
                                               int depth,
                                               int can_update,
                                               int *enabled,
                                               int32_t **frame)
{
    return sixel_interframe_prepare_shared_frame(
        dither,
        width,
        height,
        depth,
        can_update,
        SIXEL_INTERFRAME_METHOD_DIFFUSION,
        enabled,
        frame);
}

static void
sixel_interframe_diffusion_load_pixel_float32(
    sixel_dither_t *dither,
    float const *source_pixel,
    size_t base,
    int x,
    int y,
    int depth,
    int pixelformat,
    int32_t const *frame,
    float working_float[SIXEL_MAX_CHANNELS],
    unsigned char corrected[SIXEL_MAX_CHANNELS])
{
    int n;
    float interframe_delta;
    float interframe_corrected;

    (void)dither;
    (void)x;
    (void)y;

    n = 0;
    interframe_delta = 0.0f;
    interframe_corrected = 0.0f;

    for (n = 0; n < depth; ++n) {
        if (frame != NULL) {
            interframe_delta = (float)frame[base + (size_t)n]
                / (float)SIXEL_INTERFRAME_VARERR_SCALE
                / 255.0f;
        } else {
            interframe_delta = 0.0f;
        }
        interframe_corrected = source_pixel[n] + interframe_delta;
        interframe_corrected = sixel_pixelformat_float_channel_clamp(
            pixelformat,
            n,
            interframe_corrected);
        working_float[n] = interframe_corrected;
        corrected[n] = (unsigned char)sixel_pixelformat_float_channel_to_byte(
            pixelformat,
            n,
            interframe_corrected);
    }
}

static int
sixel_interframe_stbn_bias_u8_sampled_float32(
    sixel_interframe_stbn_sample_u16_fn sample_fn,
    uint32_t sequence_index,
    int x,
    int y,
    int channel,
    int depth,
    int strength_u8)
{
    uint16_t sample_value;
    int32_t bias_u8;

    sample_value = 0U;
    bias_u8 = 0;

    /*
     * Float32 keeps STBN active for both hash and mask backends in v1.
     */
    sample_value = sample_fn(sequence_index, x, y, channel, depth);
    bias_u8 = sixel_interframe_stbn_bias_u8_from_sample_u16_inline_common(
        sample_value,
        strength_u8);

    return (int)bias_u8;
}

static int
sixel_interframe_stbn_bias_u8_sampled_row_cached_float32(
    sixel_interframe_stbn_state_float32_t *stbn_state,
    int x,
    int y,
    int channel,
    int depth,
    int strength_u8)
{
    uint16_t sample_value;
    int32_t bias_u8;

    sample_value = 0U;
    bias_u8 = 0;

    sample_value = sixel_interframe_stbn_source_pmj_sample_u16_row_cached_common(
        stbn_state,
        x,
        y,
        channel,
        depth);
    bias_u8 = sixel_interframe_stbn_bias_u8_from_sample_u16_inline_common(
        sample_value,
        strength_u8);

    return (int)bias_u8;
}

static int
sixel_interframe_stbn_bias_u8_sampled_tiled_float32(
    sixel_interframe_stbn_state_float32_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth,
    int strength_u8)
{
    uint16_t sample_value;
    int32_t bias_u8;

    sample_value = 0U;
    bias_u8 = 0;

    sample_value = sixel_interframe_stbn_source_pmj_sample_u16_tiled_common(
        stbn_state,
        x,
        y,
        channel,
        depth);
    bias_u8 = sixel_interframe_stbn_bias_u8_from_sample_u16_inline_common(
        sample_value,
        strength_u8);

    return (int)bias_u8;
}

static int
sixel_interframe_stbn_prepare_pmj_float_lut_float32(
    sixel_interframe_stbn_state_float32_t *stbn_state,
    int pixelformat,
    int depth)
{
    int channel;
    int value;

    channel = 0;
    value = 0;

    if (stbn_state == NULL || depth <= 0 || depth > SIXEL_MAX_CHANNELS) {
        return 0;
    }
    if (stbn_state->pmj_float_lut_valid != 0
            && stbn_state->pmj_float_lut_pixelformat == pixelformat
            && stbn_state->pmj_float_lut_depth == depth) {
        return 1;
    }

    for (channel = 0; channel < depth; ++channel) {
        for (value = 0; value < SIXEL_INTERFRAME_FLOAT_LUT_SIZE; ++value) {
            stbn_state->pmj_float_u8_to_float[channel][value]
                = sixel_pixelformat_byte_to_float(
                    pixelformat,
                    channel,
                    (unsigned char)value);
        }
    }

    stbn_state->pmj_float_lut_pixelformat = pixelformat;
    stbn_state->pmj_float_lut_depth = depth;
    stbn_state->pmj_float_lut_valid = 1;
    return 1;
}

static SIXELSTATUS
sixel_interframe_stbn_prepare_frame_float32(sixel_dither_t *dither,
                                          int width,
                                          int height,
                                          int depth,
                                          int can_update,
                                          int *enabled,
                                          int32_t **frame)
{
    SIXELSTATUS status;
    sixel_interframe_stbn_state_float32_t *stbn_state;
    int strategy_token;
    size_t frame_bytes;
    int should_reset;
    int scene_detect_hit;

    status = SIXEL_OK;
    stbn_state = NULL;
    strategy_token = SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE;
    frame_bytes = 0U;
    should_reset = 0;
    scene_detect_hit = 0;

    status = sixel_interframe_prepare_shared_frame(
        dither,
        width,
        height,
        depth,
        can_update,
        SIXEL_INTERFRAME_METHOD_STBN,
        enabled,
        frame);
    if (status != SIXEL_OK || *enabled == 0) {
        return status;
    }

    strategy_token = sixel_interframe_strategy_token_from_dither_or_env_common(
        dither);
    status = sixel_interframe_prepare_stbn_state_common(
        dither,
        can_update,
        strategy_token,
        sizeof(sixel_interframe_stbn_state_float32_t),
        (void **)&stbn_state);
    if (status != SIXEL_OK || stbn_state == NULL || can_update == 0) {
        return status;
    }
    if (*frame != NULL
            && dither != NULL
            && dither->frame_context.valid != 0) {
        if (stbn_state->scene_cut_reset_enabled != 0
                && dither->frame_context.frame_no > 0) {
            should_reset = 1;
        } else if (stbn_state->scene_detect_enabled != 0) {
            should_reset = sixel_interframe_scene_detect_reset_float32(*frame,
                                                                       width,
                                                                       height,
                                                                       depth);
            if (should_reset != 0) {
                scene_detect_hit = 1;
            }
        }
        if (should_reset != 0) {
            frame_bytes = dither->interframe_state.error_frame_size;
            if (frame_bytes > 0U) {
                memset(*frame, 0x00, frame_bytes);
            }
            if (scene_detect_hit != 0) {
                /*
                 * Use the next STBN phase after a detected cut so the
                 * reset boundary remains visually decorrelated.
                 */
                stbn_state->sequence_index += 1U;
            }
        }
    }

    return status;
}

static void
sixel_interframe_stbn_motion_strength_u8_float32(int *strength_u8,
                                                int32_t const *frame,
                                                size_t base,
                                                int depth)
{
    int n;
    int32_t value32;
    int64_t value64;
    int64_t energy;
    int64_t max_energy;
    int scaled_u8;

    n = 0;
    value32 = 0;
    value64 = 0;
    energy = 0;
    max_energy = 0;
    scaled_u8 = 0;
    if (strength_u8 == NULL
            || *strength_u8 <= 0
            || frame == NULL
            || depth <= 0) {
        return;
    }

    for (n = 0; n < depth; ++n) {
        value32 = frame[base + (size_t)n];
        value64 = value32;
        if (value64 < 0) {
            value64 = -value64;
        }
        if (value64 > SIXEL_INTERFRAME_VARERR_MAX_VALUE) {
            value64 = SIXEL_INTERFRAME_VARERR_MAX_VALUE;
        }
        energy += value64;
    }
    max_energy = (int64_t)depth * (int64_t)SIXEL_INTERFRAME_VARERR_MAX_VALUE;
    if (max_energy <= 0) {
        return;
    }

    scaled_u8 = (int)((energy * 255 + (max_energy / 2)) / max_energy);
    if (scaled_u8 < 0) {
        scaled_u8 = 0;
    } else if (scaled_u8 > 255) {
        scaled_u8 = 255;
    }
    *strength_u8 = (*strength_u8 * scaled_u8 + 127) / 255;
}

static int
sixel_interframe_scene_detect_hit_float32(int32_t const *frame,
                                          size_t base,
                                          int depth)
{
    int n;
    int64_t value64;
    int64_t energy;
    int64_t threshold;

    n = 0;
    value64 = 0;
    energy = 0;
    threshold = (int64_t)SIXEL_INTERFRAME_VARERR_SCALE;

    if (frame == NULL || depth <= 0) {
        return 0;
    }

    for (n = 0; n < depth; ++n) {
        value64 = frame[base + (size_t)n];
        if (value64 < 0) {
            value64 = -value64;
        }
        energy += value64;
    }

    return energy >= threshold;
}

static int
sixel_interframe_stbn_perceptual_strength_u8_float32(int strength_u8,
                                                    int channel,
                                                    int depth,
                                                    int enabled)
{
    static int const weights_rgb_u8[3] = { 54, 183, 18 };

    if (enabled == 0 || strength_u8 <= 0) {
        return strength_u8;
    }
    if (depth != 3 || channel < 0 || channel >= 3) {
        return strength_u8;
    }

    return (strength_u8 * weights_rgb_u8[channel] + 127) / 255;
}

static int
sixel_interframe_alpha_guard_hit_float32(unsigned char const *transparent_mask,
                                       size_t transparent_mask_size,
                                       int width,
                                       int x,
                                       int absolute_y)
{
    int max_rows;
    size_t index;

    max_rows = 0;
    index = 0U;
    if (transparent_mask == NULL || transparent_mask_size == 0U || width <= 0
            || x < 0 || x >= width || absolute_y < 0) {
        return 0;
    }

    max_rows = (int)(transparent_mask_size / (size_t)width);
    if (absolute_y >= max_rows) {
        return 0;
    }

    if (x > 0) {
        index = (size_t)absolute_y * (size_t)width + (size_t)(x - 1);
        if (index < transparent_mask_size && transparent_mask[index] != 0U) {
            return 1;
        }
    }
    if (x + 1 < width) {
        index = (size_t)absolute_y * (size_t)width + (size_t)(x + 1);
        if (index < transparent_mask_size && transparent_mask[index] != 0U) {
            return 1;
        }
    }
    if (absolute_y > 0) {
        index = (size_t)(absolute_y - 1) * (size_t)width + (size_t)x;
        if (index < transparent_mask_size && transparent_mask[index] != 0U) {
            return 1;
        }
    }
    if (absolute_y + 1 < max_rows) {
        index = (size_t)(absolute_y + 1) * (size_t)width + (size_t)x;
        if (index < transparent_mask_size && transparent_mask[index] != 0U) {
            return 1;
        }
    }

    return 0;
}

static void
sixel_interframe_stbn_load_pixel_float32(
    sixel_dither_t *dither,
    float const *source_pixel,
    size_t base,
    int x,
    int y,
    int depth,
    int pixelformat,
    int32_t const *frame,
    float working_float[SIXEL_MAX_CHANNELS],
    unsigned char corrected[SIXEL_MAX_CHANNELS])
{
    sixel_interframe_stbn_state_float32_t *stbn_state;
    int n;
    int bias_u8;
    int adjusted_u8;
    sixel_interframe_stbn_sample_u16_fn sample_u16;
    uint32_t sequence_index;
    int use_pmj_row_cached;
    int use_pmj_tiled;
    int use_pmj_float_lut;
    int stbn_strength_u8;
    int motion_adapt_enabled;
    int scene_detect_enabled;
    int scene_detect_hit;
    int perceptual_weight_enabled;
    int fastpath_enabled;
    int channel_strength_u8;

    stbn_state = NULL;
    n = 0;
    bias_u8 = 0;
    adjusted_u8 = 0;
    sample_u16 = sixel_interframe_stbn_sample_hash_u16_common;
    sequence_index = 0U;
    use_pmj_row_cached = 0;
    use_pmj_tiled = 0;
    use_pmj_float_lut = 0;
    stbn_strength_u8 = SIXEL_INTERFRAME_STBN_V1_STRENGTH_U8;
    motion_adapt_enabled = 0;
    scene_detect_enabled = 0;
    scene_detect_hit = 0;
    perceptual_weight_enabled = 0;
    fastpath_enabled = 0;
    channel_strength_u8 = 0;

    stbn_state = (sixel_interframe_stbn_state_float32_t *)
        sixel_interframe_get_method_private(
            dither,
            SIXEL_INTERFRAME_METHOD_STBN,
            sizeof(sixel_interframe_stbn_state_float32_t));

    sixel_interframe_diffusion_load_pixel_float32(
        dither,
        source_pixel,
        base,
        x,
        y,
        depth,
        pixelformat,
        frame,
        working_float,
        corrected);

    /*
     * Cache resolved STBN sample source per pixel so per-channel bias
     * uses only the backend sample function call.
     */
    if (stbn_state != NULL) {
        sequence_index = stbn_state->sequence_index;
        stbn_strength_u8 = stbn_state->stbn_strength_u8;
        motion_adapt_enabled = stbn_state->motion_adapt_enabled != 0;
        scene_detect_enabled = stbn_state->scene_detect_enabled != 0;
        perceptual_weight_enabled =
            stbn_state->perceptual_weight_enabled != 0;
        fastpath_enabled = stbn_state->fastpath_enabled != 0;
        if (stbn_state->sample_u16 != NULL) {
            sample_u16 = stbn_state->sample_u16;
        }
        if (stbn_state->sample_source_id == SIXEL_INTERFRAME_STBN_SOURCE_PMJ) {
            if (fastpath_enabled != 0) {
                use_pmj_row_cached = 1;
                if (stbn_state->pmj_tile_enabled != 0) {
                    use_pmj_tiled = 1;
                }
                use_pmj_float_lut
                    = sixel_interframe_stbn_prepare_pmj_float_lut_float32(
                          stbn_state,
                          pixelformat,
                          depth);
            }
        }
    }

    if (motion_adapt_enabled != 0) {
        sixel_interframe_stbn_motion_strength_u8_float32(
            &stbn_strength_u8,
            frame,
            base,
            depth);
    }
    if (scene_detect_enabled != 0) {
        scene_detect_hit = sixel_interframe_scene_detect_hit_float32(frame,
                                                                     base,
                                                                     depth);
        if (scene_detect_hit != 0) {
            stbn_strength_u8 = 0;
        }
    }
    if (stbn_strength_u8 <= 0) {
        return;
    }

    if (use_pmj_tiled != 0) {
        for (n = 0; n < depth; ++n) {
            bias_u8 = sixel_interframe_stbn_bias_u8_sampled_tiled_float32(
                stbn_state,
                x,
                y,
                n,
                depth,
                sixel_interframe_stbn_perceptual_strength_u8_float32(
                    stbn_strength_u8,
                    n,
                    depth,
                    perceptual_weight_enabled));
            if (bias_u8 == 0) {
                continue;
            }

            adjusted_u8 = (int)corrected[n] + bias_u8;
            if (adjusted_u8 < 0) {
                adjusted_u8 = 0;
            } else if (adjusted_u8 > 255) {
                adjusted_u8 = 255;
            }
            corrected[n] = (unsigned char)adjusted_u8;
            if (use_pmj_float_lut != 0) {
                working_float[n] = stbn_state->pmj_float_u8_to_float[
                    n][corrected[n]];
            } else {
                working_float[n] = sixel_pixelformat_byte_to_float(
                    pixelformat,
                    n,
                    corrected[n]);
            }
        }
        return;
    }

    if (use_pmj_row_cached != 0) {
        for (n = 0; n < depth; ++n) {
            bias_u8 = sixel_interframe_stbn_bias_u8_sampled_row_cached_float32(
                stbn_state,
                x,
                y,
                n,
                depth,
                sixel_interframe_stbn_perceptual_strength_u8_float32(
                    stbn_strength_u8,
                    n,
                    depth,
                    perceptual_weight_enabled));
            if (bias_u8 == 0) {
                continue;
            }

            adjusted_u8 = (int)corrected[n] + bias_u8;
            if (adjusted_u8 < 0) {
                adjusted_u8 = 0;
            } else if (adjusted_u8 > 255) {
                adjusted_u8 = 255;
            }
            corrected[n] = (unsigned char)adjusted_u8;
            if (use_pmj_float_lut != 0) {
                working_float[n] = stbn_state->pmj_float_u8_to_float[
                    n][corrected[n]];
            } else {
                working_float[n] = sixel_pixelformat_byte_to_float(
                    pixelformat,
                    n,
                    corrected[n]);
            }
        }
        return;
    }

    for (n = 0; n < depth; ++n) {
        channel_strength_u8 =
            sixel_interframe_stbn_perceptual_strength_u8_float32(
                stbn_strength_u8,
                n,
                depth,
                perceptual_weight_enabled);
        if (channel_strength_u8 <= 0) {
            continue;
        }
        bias_u8 = sixel_interframe_stbn_bias_u8_sampled_float32(
            sample_u16,
            sequence_index,
            x,
            y,
            n,
            depth,
            channel_strength_u8);
        if (bias_u8 == 0) {
            continue;
        }

        adjusted_u8 = (int)corrected[n] + bias_u8;
        if (adjusted_u8 < 0) {
            adjusted_u8 = 0;
        } else if (adjusted_u8 > 255) {
            adjusted_u8 = 255;
        }
        corrected[n] = (unsigned char)adjusted_u8;
        working_float[n] = sixel_pixelformat_byte_to_float(
            pixelformat,
            n,
            corrected[n]);
    }
}

static sixel_interframe_method_float32_ops_t const
sixel_interframe_diffusion_ops_float32 = {
    SIXEL_INTERFRAME_METHOD_DIFFUSION,
    sixel_interframe_diffusion_prepare_frame_float32,
    sixel_interframe_diffusion_load_pixel_float32,
    sixel_interframe_clear_pixel_float32,
    sixel_interframe_store_error_float32
};

static sixel_interframe_method_float32_ops_t const
sixel_interframe_stbn_ops_float32 = {
    SIXEL_INTERFRAME_METHOD_STBN,
    sixel_interframe_stbn_prepare_frame_float32,
    sixel_interframe_stbn_load_pixel_float32,
    sixel_interframe_clear_pixel_float32,
    sixel_interframe_store_error_float32
};

static sixel_interframe_method_float32_ops_t const *
sixel_interframe_method_ops_float32_for_id(int method_id)
{
    switch (method_id) {
    case SIXEL_INTERFRAME_METHOD_DIFFUSION:
        return &sixel_interframe_diffusion_ops_float32;
    case SIXEL_INTERFRAME_METHOD_STBN:
        return &sixel_interframe_stbn_ops_float32;
    default:
        break;
    }

    return NULL;
}

static SIXELSTATUS
sixel_dither_apply_fixed_float32_with_mode(
    sixel_dither_t *dither,
    sixel_dither_context_t *context,
    int method_for_diffuse)
{
    SIXELSTATUS status;
    float *palette_float;
    float *new_palette_float;
    int float_depth;
    int serpentine;
    int y;
    int absolute_y;
    int start;
    int end;
    int step;
    int direction;
    int x;
    int pos;
    size_t base;
    float *source_pixel;
    /* Keep lookup inputs initialized across all branch combinations. */
    unsigned char quantized[SIXEL_MAX_CHANNELS] = { 0 };
    unsigned char corrected[SIXEL_MAX_CHANNELS] = { 0 };
    float working_float[SIXEL_MAX_CHANNELS] = { 0.0f };
    float lookup_pixel_float[SIXEL_MAX_CHANNELS] = { 0.0f };
    int color_index;
    int output_index;
    unsigned char palette_value_u8;
    float palette_value_float;
    float error;
    int n;
    float *data;
    unsigned char *palette;
    int float_index;
    int lookup_wants_float;
    int use_palette_float_lookup;
    int need_float_pixel;
    unsigned char const *lookup_pixel;
    int have_palette_float;
    int have_new_palette_float;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    int use_transparent_fence;
    int is_transparent;
    size_t absolute_index;
    int interframe_method;
    int interframe_enabled;
    int interframe_can_update;
    int32_t *interframe_error;
    sixel_interframe_method_float32_ops_t const *interframe_ops;
    int strategy_token;
    int interframe_spatial_diffuse;
    sixel_interframe_stbn_state_float32_t const *stbn_state;
    int stbn_alpha_guard_enabled;
    int alpha_guard_hit;

    palette_float = NULL;
    new_palette_float = NULL;
    float_depth = 0;
    interframe_method = SIXEL_INTERFRAME_METHOD_NONE;
    interframe_enabled = 0;
    interframe_can_update = 0;
    interframe_error = NULL;
    interframe_ops = NULL;
    strategy_token = SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE;
    interframe_spatial_diffuse = SIXEL_DIFFUSE_FS;
    stbn_state = NULL;
    stbn_alpha_guard_enabled = 0;
    alpha_guard_hit = 0;

    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    data = context->pixels_float;
    if (data == NULL || context->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->result == NULL || context->new_palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->migration_map == NULL || context->ncolors == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->lookup_policy == NULL || context->lookup_map == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    palette = context->palette;
    palette_float = context->palette_float;
    new_palette_float = context->new_palette_float;
    float_depth = context->float_depth;
    if (context->depth > SIXEL_MAX_CHANNELS || context->depth != 3) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->reqcolor < 1) {
        return SIXEL_BAD_ARGUMENT;
    }

    transparent_mask = context->transparent_mask;
    transparent_mask_size = context->transparent_mask_size;
    transparent_keycolor = context->transparent_keycolor;
    use_transparent_fence = 0;
    if (transparent_mask != NULL
            && transparent_keycolor >= 0
            && transparent_keycolor < SIXEL_PALETTE_MAX) {
        use_transparent_fence = 1;
    }

    serpentine = (context->method_for_scan == SIXEL_SCAN_SERPENTINE);
    lookup_wants_float = (context->lookup_source_is_float != 0);
    use_palette_float_lookup = 0;
    if (context->prefer_palette_float_lookup != 0
            && palette_float != NULL
            && float_depth >= context->depth) {
        use_palette_float_lookup = 1;
    }
    need_float_pixel = lookup_wants_float || use_palette_float_lookup;

    /*
     * Remember whether each palette buffer exposes float32 components so
     * later loops can preserve precision instead of converting back to
     * bytes before computing the diffusion error.
     */
    if (palette_float != NULL && float_depth >= context->depth) {
        have_palette_float = 1;
    } else {
        have_palette_float = 0;
    }
    if (new_palette_float != NULL && float_depth >= context->depth) {
        have_new_palette_float = 1;
    } else {
        have_new_palette_float = 0;
    }

    if (method_for_diffuse == SIXEL_DIFFUSE_INTERFRAME) {
        /*
         * Keep the interframe strategy token and spatial kernel independent
         * so callers can choose kernels with
         * -d interframe:diffusion=... or -d stbn:diffusion=....
         */
        strategy_token =
            sixel_interframe_strategy_token_from_dither_or_env_common(dither);
        interframe_method = sixel_interframe_method_from_diffuse_and_token(
            method_for_diffuse,
            strategy_token);
        interframe_ops = sixel_interframe_method_ops_float32_for_id(
            interframe_method);
        if (interframe_ops != NULL) {
            interframe_enabled = 1;
        }
        interframe_spatial_diffuse =
            sixel_interframe_spatial_diffuse_from_dither_or_env_common(
                dither);
        method_for_diffuse = interframe_spatial_diffuse;
    }
    if (context->depth != 3) {
        method_for_diffuse = SIXEL_DIFFUSE_NONE;
    }

    if (interframe_enabled) {
        if (interframe_ops == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        interframe_can_update = dither->interframe_state.last_apply_consumed;
        status = interframe_ops->prepare_frame(
            dither,
            context->width,
            context->height,
            context->depth,
            interframe_can_update,
            &interframe_enabled,
            &interframe_error);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (interframe_enabled
                && interframe_method == SIXEL_INTERFRAME_METHOD_STBN) {
            stbn_state = (sixel_interframe_stbn_state_float32_t const *)
                sixel_interframe_get_method_private_const(
                    dither,
                    SIXEL_INTERFRAME_METHOD_STBN,
                    sizeof(sixel_interframe_stbn_state_float32_t));
            if (stbn_state != NULL && stbn_state->alpha_guard_enabled != 0) {
                stbn_alpha_guard_enabled = 1;
            }
        }
    }

    if (context->optimize_palette) {
        *context->ncolors = 0;
        memset(context->new_palette, 0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)context->depth);
        if (new_palette_float != NULL && float_depth > 0) {
            memset(new_palette_float, 0x00,
                   (size_t)SIXEL_PALETTE_MAX
                       * (size_t)float_depth * sizeof(float));
        }
        memset(context->migration_map, 0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
    } else {
        *context->ncolors = context->reqcolor;
    }

#define SIXEL_DITHER_APPLY_FIXED_FLOAT32_LOOP(DIFFUSE_FN)               \
    for (y = 0; y < context->height; ++y) {                             \
        absolute_y = context->band_origin + y;                          \
        sixel_dither_scanline_params_fixed_float32(                     \
            serpentine, absolute_y, context->width,                     \
            &start, &end, &step, &direction);                           \
        for (x = start; x != end; x += step) {                          \
            pos = y * context->width + x;                               \
            base = (size_t)pos * (size_t)context->depth;                \
            is_transparent = 0;                                         \
            if (use_transparent_fence && absolute_y >= 0) {             \
                absolute_index = (size_t)absolute_y                     \
                    * (size_t)context->width                            \
                    + (size_t)x;                                        \
                if (absolute_index < transparent_mask_size              \
                        && transparent_mask[absolute_index] != 0U) {    \
                    is_transparent = 1;                                 \
                }                                                       \
            }                                                           \
            if (is_transparent) {                                       \
                if (absolute_y >= context->output_start) {              \
                    context->result[pos]                                \
                        = (sixel_index_t)transparent_keycolor;          \
                }                                                       \
                if (interframe_enabled && interframe_ops != NULL) {     \
                    interframe_ops->clear_pixel(interframe_error,       \
                                                base,                    \
                                                context->depth,          \
                                                interframe_can_update);  \
                }                                                       \
                continue;                                               \
            }                                                           \
            alpha_guard_hit = 0;                                        \
            if (stbn_alpha_guard_enabled != 0 &&                        \
                    use_transparent_fence != 0) {                       \
                alpha_guard_hit = sixel_interframe_alpha_guard_hit_float32(\
                    transparent_mask,                                   \
                            transparent_mask_size,                      \
                            context->width,                             \
                            x,                                          \
                            absolute_y);                                \
            } else if (stbn_alpha_guard_enabled != 0                    \
                    && (x == 0 || y == 0                                \
                        || x == context->width - 1                      \
                        || y == context->height - 1)) {                 \
                /*                                                       \
                 * Fall back to image-border guarding so alpha_guard    \
                 * keeps deterministic effect when transparent fences   \
                 * are absent.                                          \
                 */                                                      \
                alpha_guard_hit = 1;                                    \
            }                                                           \
            source_pixel = data + base;                                 \
                                                                        \
            /*                                                           \
             * Keep the dereference guarded even after the early        \
             * interframe setup checks so GCC -fanalyzer can prove this \
             * path is safe.                                            \
             */                                                          \
            if (interframe_enabled && interframe_ops != NULL) {         \
                if (alpha_guard_hit != 0                                \
                        && interframe_method ==                         \
                            SIXEL_INTERFRAME_METHOD_STBN) {             \
                    sixel_interframe_diffusion_load_pixel_float32(      \
                        dither,                                         \
                        source_pixel,                                   \
                        base,                                           \
                        x,                                              \
                        absolute_y,                                     \
                        context->depth,                                 \
                        context->pixelformat,                           \
                        interframe_error,                               \
                        working_float,                                  \
                        corrected);                                     \
                } else {                                                \
                    interframe_ops->load_pixel(                         \
                        dither,                                         \
                        source_pixel,                                   \
                        base,                                           \
                        x,                                              \
                        absolute_y,                                     \
                        context->depth,                                 \
                        context->pixelformat,                           \
                        interframe_error,                               \
                        working_float,                                  \
                        corrected);                                     \
                }                                                       \
                for (n = 0; n < context->depth; ++n) {                 \
                    quantized[n] = corrected[n];                        \
                    if (need_float_pixel) {                             \
                        lookup_pixel_float[n] = working_float[n];       \
                    }                                                   \
                }                                                       \
            } else {                                                    \
                for (n = 0; n < context->depth; ++n) {                 \
                    working_float[n] = source_pixel[n];                 \
                    if (!lookup_wants_float                             \
                            && !use_palette_float_lookup) {             \
                        quantized[n]                                    \
                            = sixel_pixelformat_float_channel_to_byte(  \
                                context->pixelformat,                   \
                                n,                                      \
                                source_pixel[n]);                       \
                    }                                                   \
                    if (need_float_pixel) {                             \
                        lookup_pixel_float[n] = working_float[n];       \
                    }                                                   \
                }                                                       \
            }                                                           \
                                                                        \
            if (lookup_wants_float) {                                   \
                lookup_pixel = (unsigned char const *)(void const *)    \
                    working_float;                                      \
                color_index = context->lookup_map(context->lookup_policy,\
                                                  lookup_pixel);         \
            } else if (use_palette_float_lookup) {                      \
                color_index = sixel_dither_lookup_palette_float32(      \
                    lookup_pixel_float,                                 \
                    context->depth,                                     \
                    palette_float,                                      \
                    context->reqcolor);                                 \
            } else {                                                    \
                lookup_pixel = quantized;                               \
                color_index = context->lookup_map(context->lookup_policy,\
                                                  lookup_pixel);         \
            }                                                           \
                                                                        \
            if (context->optimize_palette) {                            \
                if (context->migration_map[color_index] == 0) {         \
                    output_index = *context->ncolors;                   \
                    for (n = 0; n < context->depth; ++n) {              \
                        context->new_palette[                           \
                            output_index * context->depth + n]          \
                                = palette[color_index * context->depth  \
                                          + n];                         \
                    }                                                   \
                    if (palette_float != NULL                           \
                            && new_palette_float != NULL                \
                            && float_depth > 0) {                       \
                        for (float_index = 0;                           \
                                float_index < float_depth;              \
                                ++float_index) {                        \
                            new_palette_float[output_index * float_depth\
                                              + float_index]            \
                                = palette_float[color_index * float_depth\
                                                + float_index];         \
                        }                                               \
                    }                                                   \
                    ++*context->ncolors;                                \
                    /*                                                   \
                     * The palette count never exceeds                  \
                     * SIXEL_PALETTE_MAX (256), so storing it in an    \
                     * unsigned short is safe.                          \
                     */                                                  \
                    context->migration_map[color_index]                 \
                        = (unsigned short)(*context->ncolors);          \
                } else {                                                \
                    output_index = context->migration_map[color_index]  \
                        - 1;                                            \
                }                                                       \
                if (absolute_y >= context->output_start) {              \
                    /*                                                   \
                     * Palette indices are bounded by                   \
                     * SIXEL_PALETTE_MAX, which fits in                 \
                     * sixel_index_t (unsigned char).                   \
                     */                                                  \
                    context->result[pos] = (sixel_index_t)output_index; \
                }                                                       \
            } else {                                                    \
                output_index = color_index;                             \
                if (absolute_y >= context->output_start) {              \
                    context->result[pos] = (sixel_index_t)output_index; \
                }                                                       \
            }                                                           \
                                                                        \
            for (n = 0; n < context->depth; ++n) {                     \
                if (context->optimize_palette) {                        \
                    palette_value_u8 = context->new_palette[            \
                        output_index * context->depth + n];             \
                    if (have_new_palette_float) {                       \
                        palette_value_float =                           \
                            new_palette_float[output_index * float_depth\
                                              + n];                     \
                    } else {                                            \
                        palette_value_float                             \
                            = sixel_pixelformat_byte_to_float(          \
                                  context->pixelformat,                 \
                                  n,                                    \
                                  palette_value_u8);                    \
                    }                                                   \
                } else {                                                \
                    palette_value_u8 =                                  \
                        palette[color_index * context->depth + n];      \
                    if (have_palette_float) {                           \
                        palette_value_float =                           \
                            palette_float[color_index * float_depth     \
                                          + n];                         \
                    } else {                                            \
                        palette_value_float                             \
                            = sixel_pixelformat_byte_to_float(          \
                                  context->pixelformat,                 \
                                  n,                                    \
                                  palette_value_u8);                    \
                    }                                                   \
                }                                                       \
                error = working_float[n] - palette_value_float;         \
                if (interframe_enabled && interframe_ops != NULL) {     \
                    interframe_ops->store_error(                        \
                        interframe_error,                               \
                        base,                                           \
                        n,                                              \
                        (int)corrected[n] - (int)palette_value_u8,      \
                        interframe_can_update);                         \
                }                                                       \
                source_pixel[n] = palette_value_float;                  \
                DIFFUSE_FN(data + (size_t)n,                            \
                           context->width,                              \
                           context->height,                             \
                           x,                                           \
                           y,                                           \
                           context->depth,                              \
                           error,                                       \
                           direction,                                   \
                           context->pixelformat,                        \
                           n);                                          \
            }                                                           \
        }                                                               \
        if (absolute_y >= context->output_start) {                      \
            sixel_dither_pipeline_row_notify(dither, absolute_y);       \
        }                                                               \
    }

    switch (method_for_diffuse) {
    case SIXEL_DIFFUSE_NONE:
        SIXEL_DITHER_APPLY_FIXED_FLOAT32_LOOP(diffuse_none_float);
        break;
    case SIXEL_DIFFUSE_ATKINSON:
        SIXEL_DITHER_APPLY_FIXED_FLOAT32_LOOP(diffuse_atkinson_float);
        break;
    case SIXEL_DIFFUSE_JAJUNI:
        SIXEL_DITHER_APPLY_FIXED_FLOAT32_LOOP(diffuse_jajuni_float);
        break;
    case SIXEL_DIFFUSE_STUCKI:
        SIXEL_DITHER_APPLY_FIXED_FLOAT32_LOOP(diffuse_stucki_float);
        break;
    case SIXEL_DIFFUSE_BURKES:
        SIXEL_DITHER_APPLY_FIXED_FLOAT32_LOOP(diffuse_burkes_float);
        break;
    case SIXEL_DIFFUSE_SIERRA1:
        SIXEL_DITHER_APPLY_FIXED_FLOAT32_LOOP(diffuse_sierra1_float);
        break;
    case SIXEL_DIFFUSE_SIERRA2:
        SIXEL_DITHER_APPLY_FIXED_FLOAT32_LOOP(diffuse_sierra2_float);
        break;
    case SIXEL_DIFFUSE_SIERRA3:
        SIXEL_DITHER_APPLY_FIXED_FLOAT32_LOOP(diffuse_sierra3_float);
        break;
    case SIXEL_DIFFUSE_INTERFRAME:
        SIXEL_DITHER_APPLY_FIXED_FLOAT32_LOOP(diffuse_fs_float);
        break;
    case SIXEL_DIFFUSE_FS:
        SIXEL_DITHER_APPLY_FIXED_FLOAT32_LOOP(diffuse_fs_float);
        break;
    default:
        return SIXEL_BAD_ARGUMENT;
    }
#undef SIXEL_DITHER_APPLY_FIXED_FLOAT32_LOOP

    (void)method_for_diffuse;
    (void)strategy_token;
    (void)interframe_spatial_diffuse;

    if (context->optimize_palette) {
        memcpy(context->palette,
               context->new_palette,
               (size_t)(*context->ncolors * context->depth));
        if (palette_float != NULL
                && new_palette_float != NULL
                && float_depth > 0) {
            memcpy(palette_float,
                   new_palette_float,
                   (size_t)(*context->ncolors * float_depth)
                       * sizeof(float));
        }
    }

    status = SIXEL_OK;
    return status;
}










static SIXELSTATUS
sixel_dither_apply_interframe_float32(sixel_dither_t *dither,
                                      sixel_dither_context_t *context)
{
    return sixel_dither_apply_fixed_float32_with_mode(
        dither, context, SIXEL_DIFFUSE_INTERFRAME);
}

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_DITHER_FIXED_FLOAT32_UNUSED __attribute__((used))
#else
# define SIXEL_DITHER_FIXED_FLOAT32_UNUSED
#endif

/*
 * Keep helper symbols referenced in single-policy translation units so
 * -Wunused-function does not fire when compile-time selection removes
 * alternate code paths.
 */
static void (* const SIXEL_DITHER_FIXED_FLOAT32_UNUSED
sixel_dither_fixed_float32_keep_diffuse_fns[])(
    float *,
    int,
    int,
    int,
    int,
    int,
    float,
    int,
    int,
    int) = {
    diffuse_none_float,
    diffuse_fs_float,
    diffuse_atkinson_float,
    diffuse_jajuni_float,
    diffuse_stucki_float,
    diffuse_burkes_float,
    diffuse_sierra1_float,
    diffuse_sierra2_float,
    diffuse_sierra3_float
};

static sixel_interframe_method_float32_ops_t const *(
    * const SIXEL_DITHER_FIXED_FLOAT32_UNUSED
    sixel_dither_fixed_float32_keep_interframe_strategy[])(int) = {
    sixel_interframe_method_ops_float32_for_id
};

#undef SIXEL_DITHER_FIXED_FLOAT32_UNUSED

/*
 * IDL (internal contract)
 *
 * class DitherPolicy : IDitherPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   apply(request);
 *   supports_parallel_bands();
 * }
 */

typedef struct sixel_dither_policy_interframe_object {
    sixel_dither_policy_interface_t base;
    sixel_atomic_u32_t ref;
    int method_for_scan;
    int pixelformat;
} sixel_dither_policy_interframe_object_t;

static sixel_dither_policy_interframe_object_t *
sixel_dither_policy_interframe_from_base(
    sixel_dither_policy_interface_t *policy)
{
    return (sixel_dither_policy_interframe_object_t *)(void *)policy;
}

static sixel_dither_policy_interframe_object_t const *
sixel_dither_policy_interframe_from_base_const(
    sixel_dither_policy_interface_t const *policy)
{
    return (sixel_dither_policy_interframe_object_t const *)(void const *)
        policy;
}

static void
sixel_dither_policy_interframe_ref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_interframe_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_interframe_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_dither_policy_interframe_unref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_interframe_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_interframe_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        free(object);
    }
}

static SIXELSTATUS
sixel_dither_policy_interframe_prepare(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_prepare_request_t const *request)
{
    sixel_dither_policy_interframe_object_t *object;

    object = NULL;
    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_interframe_from_base(policy);
    object->method_for_scan = request->method_for_scan;
    object->pixelformat = request->pixelformat;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_interframe_make_effective_request(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_apply_request_t *effective)
{
    sixel_dither_policy_interframe_object_t const *object;

    object = NULL;
    if (policy == NULL || request == NULL || effective == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_interframe_from_base_const(policy);
    *effective = *request;
    effective->method_for_scan = object->method_for_scan;
    effective->pixelformat = object->pixelformat;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_interframe_build_context(
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_context_t *context,
    unsigned char scratch[SIXEL_MAX_CHANNELS],
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4],
    float new_palette_float[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS],
    unsigned short migration_map[SIXEL_PALETTE_MAX])
{
    sixel_dither_lookup_map_fn lookup_map;
    sixel_dither_t *dither;

    lookup_map = NULL;
    dither = NULL;

    if (request == NULL || context == NULL || request->lookup_policy == NULL
            || request->lookup_policy->vtbl == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (request->reqcolor < 1) {
        sixel_helper_set_additional_message(
            "sixel_dither_map_pixels: "
            "a bad argument is detected, reqcolor < 0.");
        return SIXEL_BAD_ARGUMENT;
    }

    memset(context, 0, sizeof(*context));
    context->result = request->result;
    context->width = request->width;
    context->height = request->height;
    context->band_origin = request->band_origin;
    context->output_start = request->output_start;
    context->depth = request->depth;
    context->palette = request->palette;
    context->reqcolor = request->reqcolor;
    context->new_palette = new_palette;
    context->migration_map = migration_map;
    context->ncolors = request->ncolors;
    context->scratch = scratch;
    context->lookup_policy = request->lookup_policy;
    context->pixels = request->data;
    context->pixelformat = request->pixelformat;
    context->method_for_scan = request->method_for_scan;
    context->optimize_palette = request->foptimize_palette;
    context->complexion = request->complexion;

    lookup_map = request->lookup_policy->vtbl->map_pixel;
    context->lookup_map = lookup_map;
    context->lookup_source_is_float =
        request->lookup_policy->vtbl->lookup_source_is_float(
            request->lookup_policy);
    context->prefer_palette_float_lookup =
        request->lookup_policy->vtbl->prefer_palette_float_lookup(
            request->lookup_policy);

    if (lookup_map == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_map_pixels: lookup policy is not prepared.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
        context->pixels_float = (float *)(void *)request->data;
    }

    dither = request->dither;
    if (dither != NULL && dither->palette != NULL) {
        sixel_palette_t *palette_object;
        int float_components;

        palette_object = dither->palette;
        if (palette_object->entries_float32 != NULL
                && palette_object->float_depth > 0) {
            float_components = palette_object->float_depth
                / (int)sizeof(float);
            if (float_components > 0
                    && (size_t)float_components <= SIXEL_MAX_CHANNELS) {
                context->palette_float = palette_object->entries_float32;
                context->float_depth = float_components;
                context->new_palette_float = new_palette_float;
            }
        }
    }

    if (dither != NULL
            && dither->pipeline_transparent_mask != NULL
            && dither->pipeline_transparent_keycolor >= 0
            && dither->pipeline_transparent_keycolor < SIXEL_PALETTE_MAX) {
        context->transparent_mask = dither->pipeline_transparent_mask;
        context->transparent_mask_size = dither->pipeline_transparent_mask_size;
        context->transparent_keycolor = dither->pipeline_transparent_keycolor;
    }


    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_interframe_apply(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_apply_request_t effective;
    sixel_dither_context_t context;
    unsigned char scratch[SIXEL_MAX_CHANNELS];
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4];
    float new_palette_float[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS];
    unsigned short migration_map[SIXEL_PALETTE_MAX];

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));
    memset(scratch, 0, sizeof(scratch));
    memset(new_palette, 0, sizeof(new_palette));
    memset(new_palette_float, 0, sizeof(new_palette_float));
    memset(migration_map, 0, sizeof(migration_map));

    status = sixel_dither_policy_interframe_make_effective_request(policy,
                                                           request,
                                                           &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_dither_policy_interframe_build_context(&effective,
                                                  &context,
                                                  scratch,
                                                  new_palette,
                                                  new_palette_float,
                                                  migration_map);
    if (SIXEL_FAILED(status)) {
        return status;
    }

        if (SIXEL_PIXELFORMAT_IS_FLOAT32(context.pixelformat)
            && context.pixels_float != NULL
            && context.depth == 3
            && effective.dither != NULL
            && effective.dither->prefer_float32 != 0) {
        status = sixel_dither_apply_interframe_float32(
            effective.dither,
            &context);
        if (status == SIXEL_BAD_ARGUMENT) {
            status = sixel_dither_apply_interframe_8bit(
            effective.dither,
            &context);
        }
    } else {
        status = sixel_dither_apply_interframe_8bit(
            effective.dither,
            &context);
    }

    return status;
}

static sixel_dither_policy_supports_parallel_result_t
sixel_dither_policy_interframe_supports_parallel_bands(
    sixel_dither_policy_interface_t const *policy)
{
    (void)policy;
    return 0;
}

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_interframe_vtbl = {
    sixel_dither_policy_interframe_ref,
    sixel_dither_policy_interframe_unref,
    sixel_dither_policy_interframe_prepare,
    sixel_dither_policy_interframe_apply,
    sixel_dither_policy_interframe_supports_parallel_bands
};

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_dither_policy_create_interframe(
    sixel_dither_policy_interface_t **policy)
{
    sixel_dither_policy_interframe_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_dither_policy_interframe_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    object->base.vtbl = &g_sixel_dither_policy_interframe_vtbl;
    object->ref = 1U;
    object->method_for_scan = SIXEL_SCAN_AUTO;
    object->pixelformat = SIXEL_PIXELFORMAT_RGB888;

    *policy = &object->base;
    return SIXEL_OK;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
