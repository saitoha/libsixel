/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "refresh-spatial.h"
#include "threading.h"
#include "bluenoise_64x64.h"
#include "loader-common.h"

struct sixel_refresh_spatial {
    sixel_allocator_t *allocator;
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    sixel_thread_t thread;
    int width, height, stop, pending;
    unsigned char *input, *work, *scratch;
    uint64_t queued_frame, completed_frame, cached_frame;
    int completed, cached;
    sixel_refresh_spatial_hooks_t const *hooks;
    uint32_t result[128], snapshot[128];
};

static inline unsigned char
sixel_refresh_spatial_sobel(unsigned char const *above,
    unsigned char const *row, unsigned char const *below,
    int xm, int x, int xp)
{
    int dx, dy, sum;

    dx = -above[xm] - 2 * row[xm] - below[xm]
        + above[xp] + 2 * row[xp] + below[xp];
    dy = above[xm] + 2 * above[x] + above[xp]
        - below[xm] - 2 * below[x] - below[xp];
    sum = (abs(dx) + abs(dy) + 4) / 8;
    return (unsigned char)(sum > 255 ? 255 : sum);
}

/* Average magnitudes, not signed derivatives: opposite edges must not
 * cancel. Y' uses integer Rec.709 weights and is not physical luminance.
 * The separable 9-tap box at quarter resolution covers about 36 pixels. */
void
sixel_refresh_spatial_build(unsigned char const *rgb, int w, int h,
    unsigned char *scratch, uint32_t activity[128])
{
    unsigned char *luma, *fine, *coarse, *blur, *out;
    unsigned char const *above, *row, *below;
    size_t n, p, q;
    int sw, sh, x, y, xx, yy, ym, yp;
    int i, j, sum, count, gx, gy, x0, x1, y0, y1;

    n = (size_t)w * h;
    sw = (w + 3) / 4;
    sh = (h + 3) / 4;
    luma = scratch;
    fine = luma + n;
    coarse = fine + n;
    blur = coarse + (size_t)sw * sh;
    for (p = 0; p < n; ++p)
        luma[p] = (unsigned char)((54U * rgb[p * 3] +
            183U * rgb[p * 3 + 1] + 19U * rgb[p * 3 + 2] + 128U) >> 8);
    /* Edge columns clamp their neighbours; the interior needs no clamp,
     * so its loop is straight-line and vectorizes. */
    for (y = 0; y < h; ++y) {
        ym = y ? y - 1 : y;
        yp = y + 1 < h ? y + 1 : y;
        above = luma + (size_t)ym * w;
        row = luma + (size_t)y * w;
        below = luma + (size_t)yp * w;
        out = fine + (size_t)y * w;
        out[0] = sixel_refresh_spatial_sobel(above, row, below, 0, 0,
            w > 1 ? 1 : 0);
        for (x = 1; x < w - 1; ++x)
            out[x] = sixel_refresh_spatial_sobel(above, row, below,
                x - 1, x, x + 1);
        if (w > 1)
            out[w - 1] = sixel_refresh_spatial_sobel(above, row, below,
                w - 2, w - 1, w - 1);
    }
    /* The origin of every tile is inside the image, even at an edge. */
    for (y = 0; y < sh; ++y) {
        for (x = 0; x < sw; ++x) {
            sum = 0;
            count = 0;
            j = 0;
            do {
                i = 0;
                do {
                    sum += fine[(size_t)(y * 4 + j)*w + x * 4 + i];
                    ++count;
                } while (++i < 4 && x * 4 + i < w);
            } while (++j < 4 && y * 4 + j < h);
            coarse[(size_t)y * sw + x] = (unsigned char)
                ((sum + count / 2) / count);
        }
    }
    for (y = 0; y < sh; ++y) {
        for (x = 0; x < sw; ++x) {
            sum = 0;
            for (i = -4; i <= 4; ++i) {
                xx = x + i < 0 ? 0 : x + i >= sw ? sw - 1 : x + i;
                sum += coarse[(size_t)y * sw + xx];
            }
            blur[(size_t)y * sw + x] = (unsigned char)((sum + 4)/9);
        }
    }
    for (y = 0; y < sh; ++y) {
        for (x = 0; x < sw; ++x) {
            sum = 0;
            for (j = -4; j <= 4; ++j) {
                yy = y + j < 0 ? 0 : y + j >= sh ? sh - 1 : y + j;
                sum += blur[(size_t)yy * sw + x];
            }
            coarse[(size_t)y * sw + x] = (unsigned char)((sum + 4)/9);
        }
    }
    /* A fixed 64x64 classification bounds publication/upload to 512 bytes.
     * Take the minimum over each cell, so one busy corner cannot classify
     * its entire cell as textured. Blur can still spread across boundaries. */
    memset(activity, 0, 128U * sizeof(*activity));
    for (gy = 0; gy < 64; ++gy) {
        y0 = (int)((size_t)gy * sh / 64);
        y1 = (int)((size_t)(gy + 1)*sh / 64);
        if (y1 == y0) y1 = y0 + 1;
        for (gx = 0; gx < 64; ++gx) {
            x0 = (int)((size_t)gx * sw / 64);
            x1 = (int)((size_t)(gx + 1)*sw / 64);
            if (x1 == x0) x1 = x0 + 1;
            sum = 255;
            for (y = y0; y < y1; ++y) {
                for (x = x0; x < x1; ++x) {
                    q = (size_t)y * sw + x;
                    if (coarse[q] < sum) sum = coarse[q];
                }
            }
            p = (size_t)gy * 64 + gx;
            if (sum >= 8) activity[p / 32] |= (uint32_t)1 << (p % 32);
        }
    }
}

void
sixel_refresh_spatial_apply(sixel_delta_residual_t *schedule,
    uint32_t const activity[128], unsigned int period, unsigned int strength,
    unsigned int const *band_periods)
{
    unsigned int x, y, cycle, low_cycles, mid_cycles, level, flags;
    unsigned int low_due, mid_due;
    int extend_middle;
    size_t cell;
    uint64_t frame, cycle_index;

    frame = schedule->refresh_next_frame;
    if (!frame || !period || !strength || strength > 4) return;
    extend_middle = strength >= 3;
    low_cycles = band_periods != NULL ? band_periods[0] / period : 8U;
    mid_cycles = band_periods != NULL ? band_periods[1] / period : 4U;
    low_cycles *= strength == 2 ? 4U : 2U;
    mid_cycles *= 2U;
    /* Deadlines share their phase within the shortest band period.
     * Extend the coarse cycle without moving that spatial phase. */
    cycle_index = (frame - 1U) / period;
    low_due = (unsigned int)(cycle_index % low_cycles);
    mid_due = (unsigned int)(cycle_index % mid_cycles);
    schedule->mask.tiled = strength == 4 ? 3U : 2U;
    memcpy(schedule->mask.cells, activity, sizeof(schedule->mask.cells));
    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            cell = (size_t)y * 64 + x;
            level = schedule->mask.levels[cell];
            if (level != 4U && (!extend_middle || level != 8U)) continue;
            cycle = sixel_bn64[
                ((y + (unsigned)schedule->refresh_next_origin_y + 43U)
                    & 63U)*64U +
                ((x + (unsigned)schedule->refresh_next_origin_x + 11U)
                    & 63U)] >> 2;
            flags = 0;
            if (level == 4U && low_due != cycle % low_cycles)
                flags = 128U;
            /* A due 5..8 deadline must stay at 4. Otherwise the 9..16
             * deadline can be deferred on alternate middle-band cycles. */
            if (extend_middle && (level == 8U || flags != 0U) &&
                    mid_due != cycle % mid_cycles)
                flags |= 64U;
            schedule->mask.levels[cell] = (unsigned char)(level | flags);
        }
    }
}

static int
sixel_refresh_spatial_worker(void *opaque)
{
    sixel_refresh_spatial_t *state;
    unsigned char *swap;
    uint32_t result[128];
    uint64_t frame;

    state = opaque;
    sixel_mutex_lock(&state->mutex);
    for (;;) {
        while (!state->pending && !state->stop)
            sixel_cond_wait(&state->cond, &state->mutex);
        if (state->stop) break;
        swap = state->work;
        state->work = state->input;
        state->input = swap;
        frame = state->queued_frame;
        state->pending = 0;
        sixel_mutex_unlock(&state->mutex);
        if (state->hooks && state->hooks->before)
            state->hooks->before(state->hooks->context, frame);
        sixel_refresh_spatial_build(state->work, state->width,
            state->height, state->scratch, result);
        sixel_mutex_lock(&state->mutex);
        memcpy(state->result, result, sizeof(result));
        state->completed_frame = frame;
        state->completed = 1;
        sixel_mutex_unlock(&state->mutex);
        if (state->hooks && state->hooks->published)
            state->hooks->published(state->hooks->context, frame);
        sixel_mutex_lock(&state->mutex);
    }
    sixel_mutex_unlock(&state->mutex);
    return 0;
}

void
sixel_refresh_spatial_free(sixel_refresh_spatial_t *state)
{
    if (state == NULL) return;
    sixel_mutex_lock(&state->mutex);
    state->stop = 1;
    sixel_cond_signal(&state->cond);
    sixel_mutex_unlock(&state->mutex);
    sixel_thread_join(&state->thread);
    sixel_cond_destroy(&state->cond);
    sixel_mutex_destroy(&state->mutex);
    sixel_allocator_free(state->allocator, state->input);
    sixel_allocator_free(state->allocator, state->work);
    sixel_allocator_free(state->allocator, state->scratch);
    sixel_allocator_free(state->allocator, state);
}

static SIXELSTATUS
sixel_refresh_spatial_new(sixel_refresh_spatial_t **out,
    sixel_allocator_t *allocator, unsigned char const *rgb, int w, int h,
    uint64_t frame,
    sixel_refresh_spatial_hooks_t const *hooks)
{
    sixel_refresh_spatial_t *state;
    size_t n, coarse_pixels;
    SIXELSTATUS status;

    *out = NULL;
    if (w <= 0 || h <= 0 || w > 100000 || h > 100000 ||
            (size_t)w > SIZE_MAX / (size_t)h / 8U) return SIXEL_BAD_ARGUMENT;
    n = (size_t)w * h;
    coarse_pixels = (size_t)((w + 3)/4)*((h + 3)/4);
    state = sixel_allocator_calloc(allocator, 1, sizeof(*state));
    if (state == NULL) return SIXEL_BAD_ALLOCATION;
    state->allocator = allocator;
    state->hooks = hooks;
    state->width = w;
    state->height = h;
    status = sixel_mutex_init(&state->mutex);
    if (status != SIXEL_OK) goto fail_state;
    status = sixel_cond_init(&state->cond);
    if (status != SIXEL_OK) goto fail_mutex;
    state->input = sixel_allocator_malloc(allocator, n * 3);
    state->work = sixel_allocator_malloc(allocator, n * 3);
    state->scratch = sixel_allocator_malloc(allocator,
        n * 2 + coarse_pixels * 2);
    if (!state->input || !state->work || !state->scratch) {
        status = SIXEL_BAD_ALLOCATION;
        goto fail_buffers;
    }
    /* Queue the first due frame before the worker can take the mutex.
     * The worker checks pending on entry, so no wakeup is needed. */
    if (frame % 4U == 0U) {
        memcpy(state->input, rgb, n * 3);
        state->queued_frame = frame;
        state->pending = 1;
    }
    status = sixel_thread_create(&state->thread,
        sixel_refresh_spatial_worker, state);
    if (status != SIXEL_OK) goto fail_buffers;
    *out = state;
    return SIXEL_OK;
fail_buffers:
    sixel_allocator_free(allocator, state->input);
    sixel_allocator_free(allocator, state->work);
    sixel_allocator_free(allocator, state->scratch);
    sixel_cond_destroy(&state->cond);
fail_mutex:
    sixel_mutex_destroy(&state->mutex);
fail_state:
    sixel_allocator_free(allocator, state);
    return status;
}

SIXELSTATUS
sixel_refresh_spatial_prepare(sixel_refresh_spatial_t **owner,
    sixel_allocator_t *allocator, unsigned char const *rgb,
    sixel_delta_residual_t *schedule, unsigned int period,
    unsigned int strength, unsigned int const *band_periods,
    sixel_refresh_spatial_hooks_t const *hooks)
{
    sixel_refresh_spatial_t *state;
    uint64_t frame;
    int used, queued;
    SIXELSTATUS status;

    frame = schedule->refresh_next_frame;
    state = *owner;
    /* Drain only at lifecycle boundaries. This prevents any old geometry,
     * seek epoch or in-flight result from being published in the new epoch. */
    if (state != NULL && (frame == 0 ||
            state->width != schedule->mask.width ||
            state->height != schedule->mask.height)) {
        sixel_refresh_spatial_free(state);
        state = NULL;
        *owner = NULL;
    }
    queued = 0;
    if (state == NULL) {
        status = sixel_refresh_spatial_new(owner, allocator, rgb,
            schedule->mask.width, schedule->mask.height, frame, hooks);
        if (status != SIXEL_OK) return status;
        state = *owner;
        queued = frame % 4U == 0U;
    } else if (sixel_mutex_trylock(&state->mutex)) {
        /* Ongoing submission and snapshot publication are opportunistic.
         * The worker releases the lock while computing from its input. */
        if (state->completed) {
            memcpy(state->snapshot, state->result, sizeof(state->snapshot));
            state->cached_frame = state->completed_frame;
            state->cached = 1;
        }
        if (frame % 4U == 0U) {
            memcpy(state->input, rgb, (size_t)state->width * state->height * 3);
            state->queued_frame = frame;
            state->pending = 1;
            queued = 1;
            sixel_cond_signal(&state->cond);
        }
        sixel_mutex_unlock(&state->mutex);
    }
    used = frame && state->cached && frame > state->cached_frame &&
        frame - state->cached_frame <= 8U;
    if (used)
        sixel_refresh_spatial_apply(schedule, state->snapshot, period,
            strength, band_periods);
    sixel_trace_topic_message("delta_refresh",
        "LSXGRAD1|frame=%llu|map=%llu|used=%d|queued=%d|strength=%u",
        (unsigned long long)frame,
        (unsigned long long)(state->cached ? state->cached_frame : 0U),
        used, queued, strength);
    return SIXEL_OK;
}
