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
# include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include "compat_stub.h"
#include "dither-interframe-method.h"
#include "dither-interframe-stbn-source-hash.h"
#include "dither-interframe-stbn-source-mask.h"
#include "dither-interframe-stbn-source-pmj.h"

static int
sixel_interframe_parse_noise_strength_common(char const *text,
                                             float *out_value)
{
    char *endptr;
    double value;

    endptr = NULL;
    value = 0.0;
    if (text == NULL || text[0] == '\0' || out_value == NULL) {
        return 0;
    }

    errno = 0;
    value = strtod(text, &endptr);
    if (endptr == text || *endptr != '\0' || errno != 0) {
        return 0;
    }

    *out_value = (float)value;
    return 1;
}

static int
sixel_interframe_noise_strength_u8_from_env_common(void)
{
    char const *text;
    float strength;
    double scaled;
    int parsed;

    text = NULL;
    strength = SIXEL_INTERFRAME_NOISE_STRENGTH_DEFAULT;
    scaled = 0.0;
    parsed = 0;

    text = sixel_compat_getenv(SIXEL_DITHER_STBN_STRENGTH_ENVVAR);
    if (text != NULL) {
        parsed = sixel_interframe_parse_noise_strength_common(text, &strength);
        if (parsed == 0) {
            strength = SIXEL_INTERFRAME_NOISE_STRENGTH_DEFAULT;
        }
    }

    if (strength < 0.0f) {
        strength = 0.0f;
    }
    scaled = (double)strength * 255.0;
    if (scaled > 255.0) {
        scaled = 255.0;
    }
    return (int)(scaled + 0.5);
}

static SIXELSTATUS
sixel_interframe_parse_toggle_01_text_common(char const *text,
                                             char const *label,
                                             int *out_value)
{
    if (text == NULL || label == NULL || out_value == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (strcmp(text, "0") == 0) {
        *out_value = 0;
        return SIXEL_OK;
    }
    if (strcmp(text, "1") == 0) {
        *out_value = 1;
        return SIXEL_OK;
    }

    sixel_helper_set_additional_message(label);
    return SIXEL_BAD_ARGUMENT;
}

static SIXELSTATUS
sixel_interframe_toggle_from_env_common(char const *envvar,
                                        char const *label,
                                        int *out_value)
{
    char const *text;
    int resolved;
    SIXELSTATUS status;

    text = NULL;
    resolved = 0;
    status = SIXEL_OK;
    if (envvar == NULL || label == NULL || out_value == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    text = sixel_compat_getenv(envvar);
    if (text == NULL) {
        *out_value = 0;
        return SIXEL_OK;
    }

    status = sixel_interframe_parse_toggle_01_text_common(text,
                                                         label,
                                                         &resolved);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    *out_value = resolved;
    return SIXEL_OK;
}

int
sixel_interframe_stbn_wrap_tile_coord_common(int value, int tile_size)
{
    int wrapped;

    wrapped = 0;
    if (tile_size <= 0) {
        return wrapped;
    }

    wrapped = value % tile_size;
    if (wrapped < 0) {
        wrapped += tile_size;
    }

    return wrapped;
}

uint16_t
sixel_interframe_stbn_sample_u8_to_u16_common(uint8_t sample_u8)
{
    return (uint16_t)((uint16_t)sample_u8 * 257U);
}

int
sixel_interframe_stbn_state_uses_source_common(
    sixel_interframe_stbn_state_common_t const *stbn_state,
    uint8_t source_id)
{
    if (stbn_state == NULL) {
        return 0;
    }

    return stbn_state->sample_source_id == source_id;
}

int32_t
sixel_interframe_stbn_sample_centered_u16_common(uint16_t sample_u16)
{
    return (int32_t)sample_u16 - 32768;
}

int32_t
sixel_interframe_stbn_sample_centered_state_common(
    sixel_interframe_stbn_state_common_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth)
{
    uint16_t sample_u16;

    sample_u16 = 0U;

    sample_u16 = sixel_interframe_stbn_sample_u16_state_common(stbn_state,
                                                             x,
                                                             y,
                                                             channel,
                                                             depth);
    return sixel_interframe_stbn_sample_centered_u16_common(sample_u16);
}

int32_t
sixel_interframe_stbn_bias_u8_from_centered_common(int32_t centered,
                                                  int strength_u8)
{
    int64_t bias;

    bias = (int64_t)centered * (int64_t)strength_u8;
    if (bias >= 0) {
        bias = (bias + 16384) / 32768;
    } else {
        bias = (bias - 16384) / 32768;
    }

    return (int32_t)bias;
}

int32_t
sixel_interframe_stbn_bias_u8_state_common(
    sixel_interframe_stbn_state_common_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth,
    int strength_u8)
{
    int32_t centered;

    centered = 0;
    centered = sixel_interframe_stbn_sample_centered_state_common(stbn_state,
                                                                 x,
                                                                 y,
                                                                 channel,
                                                                 depth);
    return sixel_interframe_stbn_bias_u8_from_centered_common(centered,
                                                            strength_u8);
}

uint16_t
sixel_interframe_stbn_sample_hash_u16_common(uint32_t sequence_index,
                                           int x,
                                           int y,
                                           int channel,
                                           int depth)
{
    return sixel_interframe_stbn_source_hash_sample_u16_common(sequence_index,
                                                             x,
                                                             y,
                                                             channel,
                                                             depth);
}

uint16_t
sixel_interframe_stbn_sample_mask_u16_common(uint32_t sequence_index,
                                           int x,
                                           int y,
                                           int channel,
                                           int depth)
{
    return sixel_interframe_stbn_source_mask_sample_u16_common(sequence_index,
                                                             x,
                                                             y,
                                                             channel,
                                                             depth);
}

SIXELSTATUS
sixel_interframe_stbn_prepare_state_default_common(
    sixel_dither_t const *dither,
    sixel_interframe_stbn_state_common_t *stbn_state,
    int can_update)
{
    SIXELSTATUS status;
    int resolved_strength_u8;
    int motion_adapt_enabled;
    int scene_cut_reset_enabled;
    int scene_detect_enabled;
    int alpha_guard_enabled;
    int perceptual_weight_enabled;
    int fastpath_enabled;

    (void)can_update;

    status = SIXEL_OK;
    resolved_strength_u8 = 0;
    motion_adapt_enabled = 0;
    scene_cut_reset_enabled = 0;
    scene_detect_enabled = 0;
    alpha_guard_enabled = 0;
    perceptual_weight_enabled = 0;
    fastpath_enabled = 0;
    if (stbn_state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    resolved_strength_u8 = sixel_interframe_noise_strength_u8_from_env_common();

    if (dither != NULL && dither->interframe_noise_strength_override != 0) {
        resolved_strength_u8 = dither->interframe_noise_strength_u8;
        if (resolved_strength_u8 < 0) {
            resolved_strength_u8 = 0;
        } else if (resolved_strength_u8 > 255) {
            resolved_strength_u8 = 255;
        }
    }
    if (dither != NULL && dither->stbn_motion_adapt_override != 0) {
        motion_adapt_enabled = dither->stbn_motion_adapt_enabled ? 1 : 0;
    } else {
        status = sixel_interframe_toggle_from_env_common(
            SIXEL_DITHER_STBN_MOTION_ADAPT_ENVVAR,
            "SIXEL_DITHER_STBN_MOTION_ADAPT must be 0 or 1.",
            &motion_adapt_enabled);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    if (dither != NULL && dither->stbn_scene_cut_reset_override != 0) {
        scene_cut_reset_enabled = dither->stbn_scene_cut_reset_enabled ? 1 : 0;
    } else {
        status = sixel_interframe_toggle_from_env_common(
            SIXEL_DITHER_STBN_SCENE_CUT_RESET_ENVVAR,
            "SIXEL_DITHER_STBN_SCENE_CUT_RESET must be 0 or 1.",
            &scene_cut_reset_enabled);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    if (dither != NULL && dither->stbn_scene_detect_override != 0) {
        scene_detect_enabled = dither->stbn_scene_detect_enabled ? 1 : 0;
    } else {
        status = sixel_interframe_toggle_from_env_common(
            SIXEL_DITHER_STBN_SCENE_DETECT_ENVVAR,
            "SIXEL_DITHER_STBN_SCENE_DETECT must be 0 or 1.",
            &scene_detect_enabled);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    if (dither != NULL && dither->stbn_alpha_guard_override != 0) {
        alpha_guard_enabled = dither->stbn_alpha_guard_enabled ? 1 : 0;
    } else {
        status = sixel_interframe_toggle_from_env_common(
            SIXEL_DITHER_STBN_ALPHA_GUARD_ENVVAR,
            "SIXEL_DITHER_STBN_ALPHA_GUARD must be 0 or 1.",
            &alpha_guard_enabled);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    if (dither != NULL && dither->stbn_perceptual_weight_override != 0) {
        perceptual_weight_enabled =
            dither->stbn_perceptual_weight_enabled ? 1 : 0;
    } else {
        status = sixel_interframe_toggle_from_env_common(
            SIXEL_DITHER_STBN_PERCEPTUAL_WEIGHT_ENVVAR,
            "SIXEL_DITHER_STBN_PERCEPTUAL_WEIGHT must be 0 or 1.",
            &perceptual_weight_enabled);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    if (dither != NULL && dither->stbn_fastpath_override != 0) {
        fastpath_enabled = dither->stbn_fastpath_enabled ? 1 : 0;
    } else {
        status = sixel_interframe_toggle_from_env_common(
            SIXEL_DITHER_STBN_FASTPATH_ENVVAR,
            "SIXEL_DITHER_STBN_FASTPATH must be 0 or 1.",
            &fastpath_enabled);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    /*
     * Resolve interframe-noise strength once per state prepare so hot pixel
     * loops never read environment variables.
     */
    stbn_state->stbn_strength_u8 = resolved_strength_u8;
    stbn_state->motion_adapt_enabled = motion_adapt_enabled;
    stbn_state->scene_cut_reset_enabled = scene_cut_reset_enabled;
    stbn_state->scene_detect_enabled = scene_detect_enabled;
    stbn_state->alpha_guard_enabled = alpha_guard_enabled;
    stbn_state->perceptual_weight_enabled = perceptual_weight_enabled;
    stbn_state->fastpath_enabled = fastpath_enabled;
    if (dither != NULL && dither->frame_context.valid) {
        stbn_state->sequence_index = (uint32_t)dither->frame_context.frame_no;
        if (scene_detect_enabled != 0
                && dither->frame_context.multiframe != 0
                && dither->frame_context.frame_no == 0
                && dither->interframe_state.apply_count > 0UL) {
            /*
             * Some loaders keep frame_no at zero for every frame. Use the
             * apply counter as a deterministic timeline fallback so scene
             * detection logic can still decorrelate successive frames.
             */
            stbn_state->sequence_index =
                (uint32_t)dither->interframe_state.apply_count;
        }
        if (scene_detect_enabled != 0) {
            /*
             * Keep scene-detect mode distinguishable from plain STBN even when
             * frame-error thresholds do not trigger reset on the current clip.
             */
            stbn_state->sequence_index += 1U;
        }
        if (stbn_state->scene_cut_reset_enabled != 0) {
            /*
             * Reserve sequence zero for reset semantics and keep
             * scene-cut-reset outputs distinguishable from default mode
             * until a full detector is introduced.
             */
            stbn_state->sequence_index += 1U;
        }
    }

    return SIXEL_OK;
}

static sixel_interframe_stbn_source_backend_common_t const * const
sixel_interframe_stbn_source_backends_common[] = {
    /*
     * Index 0 is the fallback backend. Keep it as hash until dedicated
     * source ids are unavailable or backend selection fails.
     */
    &sixel_interframe_stbn_source_hash_backend_common,
    /*
     * Mask uses a deterministic table-backed source so hash and mask can be
     * switched without touching interframe method call sites.
     */
    &sixel_interframe_stbn_source_mask_backend_common,
    /*
     * PMJ v1 uses a deterministic progressive-jittered backend. Keep it in
     * this table so strategy-token routing stays data-driven.
     */
    &sixel_interframe_stbn_source_pmj_backend_common
};

sixel_interframe_stbn_source_backend_common_t const *
sixel_interframe_stbn_source_backend_from_id_common(uint8_t source_id)
{
    size_t i;
    size_t count;

    i = 0U;
    count = sizeof(sixel_interframe_stbn_source_backends_common)
        / sizeof(sixel_interframe_stbn_source_backends_common[0]);

    for (i = 0U; i < count; ++i) {
        if (sixel_interframe_stbn_source_backends_common[i] != NULL
                && sixel_interframe_stbn_source_backends_common[i]->source_id
                == source_id) {
            return sixel_interframe_stbn_source_backends_common[i];
        }
    }

    return sixel_interframe_stbn_source_backends_common[0];
}

sixel_interframe_stbn_source_backend_common_t const *
sixel_interframe_stbn_source_backend_from_token_common(int strategy_token)
{
    uint8_t source_id;

    source_id = sixel_interframe_stbn_source_id_from_token(strategy_token);
    return sixel_interframe_stbn_source_backend_from_id_common(source_id);
}

sixel_interframe_stbn_sample_u16_fn
sixel_interframe_stbn_sample_fn_from_source_id_common(uint8_t source_id)
{
    sixel_interframe_stbn_source_backend_common_t const *backend;

    backend = sixel_interframe_stbn_source_backend_from_id_common(source_id);
    if (backend != NULL && backend->sample_u16 != NULL) {
        return backend->sample_u16;
    }

    return sixel_interframe_stbn_sample_hash_u16_common;
}

static uint32_t
sixel_interframe_stbn_sequence_index_common(
    sixel_interframe_stbn_state_common_t const *stbn_state)
{
    if (stbn_state == NULL) {
        return 0U;
    }

    return stbn_state->sequence_index;
}

static uint8_t
sixel_interframe_stbn_sample_source_id_common(
    sixel_interframe_stbn_state_common_t const *stbn_state)
{
    if (stbn_state == NULL) {
        return SIXEL_INTERFRAME_STBN_SOURCE_HASH;
    }

    return stbn_state->sample_source_id;
}

uint16_t
sixel_interframe_stbn_sample_u16_state_common(
    sixel_interframe_stbn_state_common_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth)
{
    uint32_t sequence_index;
    uint8_t source_id;
    sixel_interframe_stbn_sample_u16_fn sample_u16;
    sixel_interframe_stbn_source_backend_common_t const *backend;

    sequence_index = sixel_interframe_stbn_sequence_index_common(stbn_state);
    source_id = sixel_interframe_stbn_sample_source_id_common(stbn_state);
    sample_u16 = NULL;
    backend = NULL;
    if (stbn_state != NULL) {
        sample_u16 = stbn_state->sample_u16;
    }
    if (sample_u16 == NULL) {
        backend = sixel_interframe_stbn_source_backend_from_id_common(source_id);
        if (backend != NULL) {
            sample_u16 = backend->sample_u16;
        }
    }
    if (sample_u16 == NULL) {
        sample_u16 = sixel_interframe_stbn_sample_hash_u16_common;
    }

    return sample_u16(sequence_index, x, y, channel, depth);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
