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
#include <math.h>
#include <string.h>
#include <sixel.h>
#include "delta-residual.h"
#include "bluenoise_64x64.h"

/* Filter signed components before measuring distance: alternating dither
 * errors cancel, while persistent low-frequency color offsets survive.
 * Clip and renormalize at image edges and invalid history, never fill missing
 * target samples with displayed colors or black. Sigma is four pixels. */
static double
sixel_delta_filtered_error(unsigned char const *target,
    unsigned char const *acc, unsigned char const *valid,
    int width, int height, int x, int y, double const *kernel)
{
    double sum[3];
    double weight;
    double total;
    size_t position;
    int dx, dy, sx, sy, channel;

    memset(sum, 0, sizeof(sum));
    total = 0.0;
    for (dy = -12; dy <= 12; ++dy) {
        sy = y + dy;
        if (sy < 0 || sy >= height) continue;
        for (dx = -12; dx <= 12; ++dx) {
            sx = x + dx;
            if (sx < 0 || sx >= width) continue;
            position = (size_t)sy * (size_t)width + (size_t)sx;
            if (!valid[position]) continue;
            weight = kernel[dx + 12] * kernel[dy + 12];
            total += weight;
            for (channel = 0; channel < 3; ++channel) {
                sum[channel] += weight *
                    ((double)target[position * 3U + channel] -
                     (double)acc[position * 3U + channel]);
            }
        }
    }
    if (total == 0.0) return 0.0;
    return sqrt((2.0 * sum[0] * sum[0] + 4.0 * sum[1] * sum[1] +
        sum[2] * sum[2]) / 7.0) / total;
}

void
sixel_delta_residual_prepare(sixel_delta_residual_t *state,
    unsigned char const *target, unsigned char const *acc,
    unsigned char const *valid, int width, int height,
    double threshold, unsigned int frames, unsigned int keep_threshold)
{
    double kernel[25];
    double norm, error;
    size_t cell, position;
    unsigned int gx, gy, columns, rows;
    int offset, x0, x1, y0, y1, x, y;

    memset(state->next_age, 0, sizeof(state->next_age));
    memset(&state->mask, 0, sizeof(state->mask));
    state->mask.width = width;
    state->mask.height = height;
    state->pending = 1;
    state->selected_pixels = 0U;
    /* A missing baseline or T0 observation starts a fresh sequence. */
    if (target == NULL || acc == NULL || valid == NULL) return;
    columns = width < 64 ? (unsigned int)width : 64U;
    rows = height < 64 ? (unsigned int)height : 64U;
    state->mask.columns = columns;
    state->mask.rows = rows;
    state->mask.width = width;
    state->mask.height = height;
    state->mask.threshold = keep_threshold;
    norm = 0.0;
    for (offset = -12; offset <= 12; ++offset) {
        kernel[offset + 12] = exp(-(double)(offset * offset) / 32.0);
        norm += kernel[offset + 12];
    }
    for (offset = 0; offset < 25; ++offset) kernel[offset] /= norm;
    for (gy = 0; gy < rows; ++gy) {
        y0 = (int)((size_t)gy * height / rows);
        y1 = (int)((size_t)(gy + 1U) * height / rows);
        y = y0 + (y1 - y0) / 2;
        for (gx = 0; gx < columns; ++gx) {
            x0 = (int)((size_t)gx * width / columns);
            x1 = (int)((size_t)(gx + 1U) * width / columns);
            x = x0 + (x1 - x0) / 2;
            cell = (size_t)gy * columns + gx;
            position = (size_t)y * width + x;
            if (!valid[position]) continue;
            error = sixel_delta_filtered_error(target, acc, valid,
                width, height, x, y, kernel);
            /* Guard exact constant boundaries against filter roundoff. */
            if (!(error > threshold + 1e-10)) continue;
            state->next_age[cell] = state->width == width &&
                state->height == height ? state->age[cell] : 0;
            if (state->next_age[cell] < frames) ++state->next_age[cell];
            if (state->next_age[cell] < frames) continue;
            state->mask.cells[cell / 32U] |= (uint32_t)1 << (cell % 32U);
            state->selected_pixels += (size_t)(x1 - x0) * (y1 - y0);
        }
    }
}

unsigned int
sixel_delta_mask_threshold(sixel_delta_mask_t const *mask,
    int x, int y, unsigned int threshold)
{
    size_t cell;

    if (mask == NULL || mask->columns == 0 || mask->rows == 0 ||
            x < 0 || y < 0 || x >= mask->width || y >= mask->height) {
        return threshold;
    }
    if (mask->tiled) {
        cell = ((unsigned int)y & 63U) * 64U + ((unsigned int)x & 63U);
    } else {
        cell = (((size_t)(y + 1) * mask->rows - 1U) / mask->height) *
            mask->columns +
            ((size_t)(x + 1) * mask->columns - 1U) / mask->width;
    }
    if ((mask->cells[cell / 32U] & ((uint32_t)1 << (cell % 32U))) &&
            mask->threshold < threshold) return mask->threshold;
    return threshold;
}

void
sixel_delta_refresh_prepare(sixel_delta_residual_t *state,
    int width, int height, unsigned int period_ms, uint64_t time_us,
    int history_valid)
{
    uint64_t period, previous, current, offset, epoch;
    uint32_t phases[2];
    unsigned int gx, gy, phase;
    size_t cell, nx, ny;
    int reset, selected;

    memset(&state->mask, 0, sizeof(state->mask));
    memset(state->next_age, 0, sizeof(state->next_age));
    state->selected_pixels = 0U;
    state->pending = 1;
    state->mask.width = width;
    state->mask.height = height;
    state->refresh_next_us = time_us;
    state->refresh_next_valid = 1;
    reset = !state->refresh_valid || !history_valid ||
        state->width != width || state->height != height ||
        time_us < state->refresh_last_us;
    epoch = reset ? time_us : state->refresh_epoch_us;
    state->refresh_next_epoch_us = epoch;
    if (period_ms == 0U || width <= 0 || height <= 0) {
        state->refresh_next_valid = 0;
        return;
    }
    period = (uint64_t)period_ms * 1000U;
    previous = reset ? 0U : state->refresh_last_us - epoch;
    current = time_us - epoch;
    state->mask.columns = 64U;
    state->mask.rows = 64U;
    state->mask.tiled = 1U;
    memset(phases, 0, sizeof(phases));
    for (phase = 0U; phase < 64U; ++phase) {
        offset = ((uint64_t)phase + 1U) * period / 64U;
        selected = reset ? history_valid : current >= offset &&
            (previous < offset || (current - offset) / period >
                                  (previous - offset) / period);
        if (selected) phases[phase / 32U] |= (uint32_t)1 << (phase % 32U);
    }
    /* A pixel-scale blue-noise tile avoids coherent block boundaries on
     * smooth fades. Fixed phases keep the deadline bound; independently
     * rerandomizing each frame could indefinitely miss individual pixels.
     * Translate the tile to avoid sharing phases with color dithering. */
    for (gy = 0U; gy < 64U; ++gy) {
        ny = (size_t)height / 64U + (gy < (unsigned int)height % 64U);
        for (gx = 0U; gx < 64U; ++gx) {
            phase = sixel_bn64[((gy + 17U) & 63U) * 64U +
                              ((gx + 37U) & 63U)] >> 2;
            if (!(phases[phase / 32U] & ((uint32_t)1 << (phase % 32U))))
                continue;
            cell = (size_t)gy * 64U + gx;
            state->mask.cells[cell / 32U] |= (uint32_t)1 << (cell % 32U);
            /* Count repetitions and clipped edges without scanning source
             * pixels. CPU and Metal consume this same 512-byte tile. */
            nx = (size_t)width / 64U + (gx < (unsigned int)width % 64U);
            state->selected_pixels += nx * ny;
        }
    }
}

int
sixel_delta_rgb241_within(unsigned char const *a, unsigned char const *b,
    unsigned int threshold)
{
    int dr, dg, db;

    dr = (int)a[0] - b[0];
    dg = (int)a[1] - b[1];
    db = (int)a[2] - b[2];
    return (unsigned int)(2 * dr * dr + 4 * dg * dg + db * db) <=
        7U * threshold * threshold;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
