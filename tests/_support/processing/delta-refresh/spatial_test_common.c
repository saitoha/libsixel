/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/refresh-spatial.h"
#include "src/threading.h"
#include "src/encoder.h"

#define RS_CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "spatial line %d: %s\n", __LINE__, #x); \
    return 1; } } while (0)

static int
spatial_cadence_for_period(int active, unsigned int strength,
    unsigned int period)
{
    sixel_delta_residual_t s, baseline;
    uint32_t bits[128];
    unsigned int last[64 * 64], f, x, y, t, original, span, step;
    size_t cell;

    memset(&s, 0, sizeof(s));
    memset(last, 0, sizeof(last));
    memset(bits, active ? 255 : 0, sizeof(bits));
    step = period * (active ? 8U << strength : 8U);
    span = step * 2U;
    s.width = s.height = 64;
    s.refresh_valid = 1;
    s.refresh_origin_x = 9;
    s.refresh_origin_y = 11;
    for (f = 1; f <= span; ++f) {
        s.refresh_frame = f - 1;
        sixel_delta_refresh_prepare(&s, 64, 64, 9, 11, period, 1);
        sixel_delta_refresh_adapt(&s, 9, 11, period, 4, 1);
        baseline = s;
        sixel_refresh_spatial_apply(&s, bits, period, strength);
        for (y = 0; y < 64; ++y) {
            for (x = 0; x < 64; ++x) {
                t = sixel_delta_mask_threshold(&s.mask, x, y, 255);
                original = sixel_delta_mask_threshold(&baseline.mask,
                    x, y, 255);
                /* All thresholds except a due low-band cutoff are exact,
                 * including the ordinary T when no deadline is due. */
                RS_CHECK(t == original || (active && original == 4 && t == 8));
                if (t == 4) {
                    cell = (size_t)y * 64 + x;
                    RS_CHECK(last[cell] ? f - last[cell] == step : f <= step);
                    last[cell] = f;
                }
                RS_CHECK(sixel_delta_mask_threshold(&s.mask, x, y, 2) == 2);
            }
        }
    }
    for (cell = 0; cell < 4096; ++cell) RS_CHECK(last[cell] > step);
    return 0;
}

int
test_refresh_spatial_cadence(int active, unsigned int strength)
{
    RS_CHECK(spatial_cadence_for_period(active, strength, 1U) == 0);
    RS_CHECK(spatial_cadence_for_period(active, strength, 5U) == 0);
    return 0;
}

int
test_refresh_spatial_map(int textured)
{
    unsigned char rgb[64 * 64 * 3], scratch[64 * 64 * 2 + 16 * 16 * 2];
    uint32_t bits[128];
    int x, y, c;

    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            for (c = 0; c < 3; ++c)
                rgb[(y * 64 + x)*3 + c] = (unsigned char)
                    (textured ? ((x / 4)%2 ? 255 : 0) : 100);
        }
    }
    sixel_refresh_spatial_build(rgb, 64, 64, scratch, bits);
    for (x = 0; x < 128; ++x)
        RS_CHECK(bits[x] == (textured ? UINT32_MAX : 0U));
    return 0;
}

typedef struct spatial_barrier {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    int entered, release, published, snapshot;
    uint64_t frame;
} spatial_barrier_t;

static void
spatial_before(void *opaque, uint64_t frame)
{
    spatial_barrier_t *gate;

    (void)frame;
    gate = opaque;
    sixel_mutex_lock(&gate->mutex);
    gate->entered = 1;
    sixel_cond_broadcast(&gate->cond);
    while (!gate->release) sixel_cond_wait(&gate->cond, &gate->mutex);
    sixel_mutex_unlock(&gate->mutex);
}

static void
spatial_published(void *opaque, uint64_t frame)
{
    spatial_barrier_t *gate;

    gate = opaque;
    sixel_mutex_lock(&gate->mutex);
    gate->frame = frame;
    gate->published = 1;
    sixel_cond_broadcast(&gate->cond);
    while (frame == 8 && !gate->snapshot)
        sixel_cond_wait(&gate->cond, &gate->mutex);
    sixel_mutex_unlock(&gate->mutex);
}

/* The worker is held outside its queue lock. An ordinary submission must
 * return with A1, and pending input must be latest-only. A condition barrier
 * proves publication; no sleeping or scheduler-speed assumptions are used. */
int
test_refresh_spatial_async(int stale)
{
    sixel_refresh_spatial_t *state;
    sixel_allocator_t *allocator;
    sixel_delta_residual_t s;
    sixel_refresh_spatial_hooks_t hooks;
    spatial_barrier_t gate;
    unsigned char rgb[64 * 64 * 3];
    int failed, initialized;

    state = NULL;
    allocator = NULL;
    failed = 0;
    initialized = 0;
    memset(&gate, 0, sizeof(gate));
    memset(&s, 0, sizeof(s));
    memset(rgb, 100, sizeof(rgb));
    hooks.before = spatial_before;
    hooks.published = spatial_published;
    hooks.context = &gate;
    RS_CHECK(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)
        == SIXEL_OK);
    RS_CHECK(sixel_mutex_init(&gate.mutex) == SIXEL_OK);
    RS_CHECK(sixel_cond_init(&gate.cond) == SIXEL_OK);
    initialized = 1;
    s.mask.width = s.mask.height = 64;
    if (sixel_refresh_spatial_prepare(&state, allocator, rgb, &s, 1, 1, &hooks)
            != SIXEL_OK) { failed = 1; goto end; }
    sixel_mutex_lock(&gate.mutex);
    while (!gate.entered) sixel_cond_wait(&gate.cond, &gate.mutex);
    sixel_mutex_unlock(&gate.mutex);
    s.refresh_next_frame = 4;
    if (sixel_refresh_spatial_prepare(&state, allocator, rgb, &s, 1, 1, &hooks)
            != SIXEL_OK || s.mask.tiled == 2) failed = 1;
    s.refresh_next_frame = 8;
    if (sixel_refresh_spatial_prepare(&state, allocator, rgb, &s, 1, 1, &hooks)
            != SIXEL_OK || s.mask.tiled == 2) failed = 1;
    sixel_mutex_lock(&gate.mutex);
    gate.release = 1;
    sixel_cond_broadcast(&gate.cond);
    while (!gate.published || gate.frame != 8)
        sixel_cond_wait(&gate.cond, &gate.mutex);
    sixel_mutex_unlock(&gate.mutex);
    s.refresh_next_frame = stale ? 17 : 9;
    if (sixel_refresh_spatial_prepare(&state, allocator, rgb, &s, 1, 1, &hooks)
            != SIXEL_OK || (s.mask.tiled == 2) == stale) failed = 1;
    sixel_mutex_lock(&gate.mutex);
    gate.snapshot = 1;
    sixel_cond_broadcast(&gate.cond);
    sixel_mutex_unlock(&gate.mutex);
    /* Reset drains old geometry and cannot reuse its completed map. */
    s.refresh_next_frame = 0;
    s.mask.tiled = 1;
    s.mask.width = 32;
    if (sixel_refresh_spatial_prepare(&state, allocator, rgb, &s, 1, 1, &hooks)
            != SIXEL_OK || s.mask.tiled != 1) failed = 1;
end:
    sixel_refresh_spatial_free(state);
    if (initialized) {
        sixel_cond_destroy(&gate.cond);
        sixel_mutex_destroy(&gate.mutex);
    }
    sixel_allocator_unref(allocator);
    return failed;
}

int
test_refresh_spatial_option(char const *value, int accepted)
{
    sixel_encoder_t *encoder;
    SIXELSTATUS status;

    RS_CHECK(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY, value);
    sixel_encoder_unref(encoder);
    RS_CHECK((status == SIXEL_OK) == accepted);
    return 0;
}
#undef RS_CHECK
