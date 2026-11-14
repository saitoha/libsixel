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

/* Average magnitudes, not signed derivatives: opposite edges must not
 * cancel. Y' uses integer Rec.709 weights and is not physical luminance.
 * The separable 9-tap box at quarter resolution covers about 36 pixels. */
void
sixel_refresh_spatial_build(unsigned char const *rgb, int w, int h,
    unsigned char *scratch, uint32_t activity[128])
{
    unsigned char *luma, *fine, *small, *blur;
    size_t n, p, q;
    int sw, sh, x, y, xx, yy, xm, xp, ym, yp, dx, dy;
    int i, j, sum, count, gx, gy, x0, x1, y0, y1;

    n = (size_t)w * h;
    sw = (w + 3) / 4;
    sh = (h + 3) / 4;
    luma = scratch;
    fine = luma + n;
    small = fine + n;
    blur = small + (size_t)sw * sh;
    for (p = 0; p < n; ++p)
        luma[p] = (unsigned char)((54U * rgb[p * 3] +
            183U * rgb[p * 3 + 1] + 19U * rgb[p * 3 + 2] + 128U) >> 8);
    for (y = 0; y < h; ++y) {
        ym = y ? y - 1 : y;
        yp = y + 1 < h ? y + 1 : y;
        for (x = 0; x < w; ++x) {
            xm = x ? x - 1 : x;
            xp = x + 1 < w ? x + 1 : x;
            dx = -luma[(size_t)ym * w + xm] - 2 * luma[(size_t)y * w + xm]
                - luma[(size_t)yp * w + xm] + luma[(size_t)ym * w + xp]
                + 2 * luma[(size_t)y * w + xp] + luma[(size_t)yp * w + xp];
            dy = luma[(size_t)ym * w + xm] + 2 * luma[(size_t)ym * w + x]
                + luma[(size_t)ym * w + xp] - luma[(size_t)yp * w + xm]
                - 2 * luma[(size_t)yp * w + x] - luma[(size_t)yp * w + xp];
            sum = (abs(dx) + abs(dy) + 4) / 8;
            fine[(size_t)y * w + x] = (unsigned char)(sum > 255 ? 255 : sum);
        }
    }
    for (y = 0; y < sh; ++y) {
        for (x = 0; x < sw; ++x) {
            sum = 0;
            count = 0;
            for (j = 0; j < 4 && y * 4 + j < h; ++j) {
                for (i = 0; i < 4 && x * 4 + i < w; ++i) {
                    sum += fine[(size_t)(y * 4 + j)*w + x * 4 + i];
                    ++count;
                }
            }
            small[(size_t)y * sw + x] = (unsigned char)
                ((sum + count / 2) / count);
        }
    }
    for (y = 0; y < sh; ++y) {
        for (x = 0; x < sw; ++x) {
            sum = 0;
            for (i = -4; i <= 4; ++i) {
                xx = x + i < 0 ? 0 : x + i >= sw ? sw - 1 : x + i;
                sum += small[(size_t)y * sw + xx];
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
            small[(size_t)y * sw + x] = (unsigned char)((sum + 4)/9);
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
                    if (small[q] < sum) sum = small[q];
                }
            }
            p = (size_t)gy * 64 + gx;
            if (sum >= 8) activity[p / 32] |= (uint32_t)1 << (p % 32);
        }
    }
}

void
sixel_refresh_spatial_apply(sixel_delta_residual_t *schedule,
    uint32_t const activity[128], unsigned int period, unsigned int strength)
{
    unsigned int x, y, cycle, cycles, phase;
    size_t cell;
    uint64_t frame;

    frame = schedule->refresh_next_frame;
    if (!frame || !period || !strength || strength > 2) return;
    cycles = 8U << strength;
    /* A1 deadlines are on frames base + n*J, with base in 1..J.
     * Test the coarse cycle n to preserve the within-J blue-noise phase. */
    phase = (unsigned int)(((frame - 1U) / period) & (cycles - 1U));
    schedule->mask.tiled = 2U;
    memcpy(schedule->mask.cells, activity, sizeof(schedule->mask.cells));
    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            cell = (size_t)y * 64 + x;
            if (schedule->mask.levels[cell] != 4U) continue;
            cycle = sixel_bn64[
                ((y + (unsigned)schedule->refresh_next_origin_y + 43U)
                    & 63U)*64U +
                ((x + (unsigned)schedule->refresh_next_origin_x + 11U)
                    & 63U)] >> 2;
            if (phase != (cycle & (cycles - 1U)))
                schedule->mask.levels[cell] |= 128U;
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
    sixel_allocator_t *allocator, int w, int h,
    sixel_refresh_spatial_hooks_t const *hooks)
{
    sixel_refresh_spatial_t *state;
    size_t n, small;
    SIXELSTATUS status;

    *out = NULL;
    if (w <= 0 || h <= 0 || w > 100000 || h > 100000 ||
            (size_t)w > SIZE_MAX / (size_t)h / 8U) return SIXEL_BAD_ARGUMENT;
    n = (size_t)w * h;
    small = (size_t)((w + 3)/4)*((h + 3)/4);
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
    state->scratch = sixel_allocator_malloc(allocator, n * 2 + small * 2);
    if (!state->input || !state->work || !state->scratch) {
        status = SIXEL_BAD_ALLOCATION;
        goto fail_buffers;
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
    sixel_delta_residual_t *schedule, unsigned int period, unsigned int strength,
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
    if (state == NULL) {
        status = sixel_refresh_spatial_new(owner, allocator,
            schedule->mask.width, schedule->mask.height, hooks);
        if (status != SIXEL_OK) return status;
        state = *owner;
    }
    queued = 0;
    /* Both submission and snapshot publication are opportunistic. The
     * worker never holds this lock while computing or reading its input. */
    if (sixel_mutex_trylock(&state->mutex)) {
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
        sixel_refresh_spatial_apply(schedule, state->snapshot, period, strength);
    sixel_trace_topic_message("delta_refresh",
        "LSXGRAD1|frame=%llu|map=%llu|used=%d|queued=%d|strength=%u",
        (unsigned long long)frame,
        (unsigned long long)(state->cached ? state->cached_frame : 0U),
        used, queued, strength);
    return SIXEL_OK;
}
