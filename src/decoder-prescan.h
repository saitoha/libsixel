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
    unsigned int flags;
    size_t *band_start_offsets;
    size_t *band_end_offsets;
    sixel_prescan_band_state_t *band_states;
    sixel_prescan_band_state_t final_state;
} sixel_prescan_t;

SIXELSTATUS sixel_prescan_run(unsigned char *p,
                              int len,
                              sixel_prescan_t **out_prescan,
                              sixel_allocator_t *allocator);

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
