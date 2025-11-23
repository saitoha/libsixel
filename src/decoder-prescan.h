/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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

#ifndef SIXEL_DECODER_PRESCAN_H
#define SIXEL_DECODER_PRESCAN_H

#include <stddef.h>
#include <stdint.h>

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Lightweight prescan metadata used by the parallel decoder. The prescan
 * gathers band boundaries and parser state snapshots without rendering
 * pixels, so each worker can resume decoding from the correct position.
 */

#ifndef DECSIXEL_PARAMS_MAX
#define DECSIXEL_PARAMS_MAX 16
#endif

#ifndef SIXEL_PALETTE_MAX_DECODER
#define SIXEL_PALETTE_MAX_DECODER 65536
#endif

#define SIXEL_PRESCAN_PALETTE_MAX 256

#define SIXEL_PRESCAN_FLAG_UNSAFE_GEOMETRY    (1u << 0)
#define SIXEL_PRESCAN_FLAG_UNSUPPORTED_COLOR  (1u << 1)

typedef enum sixel_band_event_type {
    SIXEL_BAND_EVENT_PEN = 0,
    SIXEL_BAND_EVENT_PALETTE = 1,
    SIXEL_BAND_EVENT_RASTER_ATTR = 2,
    SIXEL_BAND_EVENT_BACKGROUND = 3
} sixel_band_event_type_t;

typedef struct sixel_band_event {
    sixel_band_event_type_t type;
    int color_index;
    int pad;
    int pan;
    int ph;
    int pv;
    uint32_t color;
} sixel_band_event_t;

typedef enum sixel_prescan_parse_state {
    SIXEL_PRESCAN_PS_GROUND   = 0,
    SIXEL_PRESCAN_PS_ESC      = 1,
    SIXEL_PRESCAN_PS_DCS      = 2,
    SIXEL_PRESCAN_PS_DECSIXEL = 3,
    SIXEL_PRESCAN_PS_DECGRA   = 4,
    SIXEL_PRESCAN_PS_DECGRI   = 5,
    SIXEL_PRESCAN_PS_DECGCI   = 6
} sixel_prescan_parse_state_t;

typedef struct sixel_prescan_band_state {
    sixel_prescan_parse_state_t state;
    int pos_x;
    int pos_y;
    int repeat_count;
    int color_index;
    int bgindex;
    int attributed_pan;
    int attributed_pad;
    int attributed_ph;
    int attributed_pv;
    int param;
    int nparams;
    int params[DECSIXEL_PARAMS_MAX];
    int p2_background;
    int par_num;
    int par_den;
    uint32_t palette[SIXEL_PRESCAN_PALETTE_MAX];
} sixel_prescan_band_state_t;

typedef struct sixel_prescan {
    int width;
    int height;
    int band_count;
    int band_capacity;
    int discard_bands;
    int *band_repeat_sums; /* repeat hints per band for weighting */
    int *band_token_counts; /* token counts per band for weighting */
    int *band_event_starts; /* prefix count of events at band start */
    int event_count;
    int event_capacity;
    sixel_band_event_t *events;
    unsigned int flags;
    size_t *band_start_offsets;
    size_t *band_end_offsets;
    sixel_prescan_band_state_t *band_states;
    sixel_prescan_band_state_t final_state;
} sixel_prescan_t;

/*
 * Optional hooks used to observe band boundaries while the prescan walks the
 * SIXEL stream. The prescan itself still owns all buffers; callbacks can read
 * the metadata already captured in the prescan structure and mirror it
 * elsewhere. The event hook fires after each palette/pen/raster/background
 * update has been recorded. The finish hook runs after the final band is
 * sealed so callers can cache the terminal state and geometry without
 * retaining the prescan tables. Set discard_events when on_event mirrors every
 * event and the caller does not need the prescan-owned event log. Set
 * discard_bands when on_band consumes all per-band metadata so the prescan
 * can skip storing band tables beyond the first snapshot.
 * Both discard flags require their matching callbacks to avoid losing
 * metadata.
 *
 * When out_prescan is NULL the prescan operates in callback-only mode: all
 * metadata must be consumed from the hooks because the prescan tables are
 * destroyed before returning. Callers need to provide geometry and final
 * palette through the callbacks in that configuration.
 */
typedef struct sixel_prescan_callbacks {
    SIXELSTATUS (*on_band)(sixel_prescan_t *prescan,
                           int band_index,
                           sixel_prescan_band_state_t const *band_state,
                           size_t start_offset,
                           size_t end_offset,
                           int event_start,
                           int weight_tokens,
                           int weight_repeats,
                           void *user_data);
    SIXELSTATUS (*on_event)(sixel_prescan_t *prescan,
                            int event_index,
                            sixel_band_event_t const *event,
                            void *user_data);
    SIXELSTATUS (*on_finish)(sixel_prescan_t *prescan,
                             sixel_prescan_band_state_t const *final_state,
                             int width,
                             int height,
                             int bgindex,
                             int band_count,
                             unsigned int flags,
                             void *user_data);
    int discard_bands;
    int discard_events;
    void *user_data;
} sixel_prescan_callbacks_t;

SIXELSTATUS sixel_prescan_run(unsigned char *p,
                              int len,
                              sixel_prescan_t **out_prescan,
                              sixel_allocator_t *allocator,
                              sixel_prescan_callbacks_t const *callbacks);

void sixel_prescan_destroy(sixel_prescan_t *prescan,
                           sixel_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif /* SIXEL_DECODER_PRESCAN_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
