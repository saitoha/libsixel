/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/encoder.h"
#include "playback_test_common.h"

#define LIFE_CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "playback lifecycle %d line %d: %s\n", \
        mode, __LINE__, #x); goto end; } } while (0)

#if SIXEL_ENABLE_THREADS
struct lifecycle_state {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    sixel_playback_t *playback;
    sixel_playback_ticket_t ticket;
    SIXELSTATUS result;
    SIXELSTATUS completed[4];
    int write_entered;
    int wait_entered;
    int open;
    int fail_write;
    int count;
    int bad_sequence;
    int gate_failed;
};

/* The timeout is only a failure guard, never the source of ordering. */
static int
lifecycle_wait(struct lifecycle_state *s, int *value)
{
    while (!*value) {
        if (sixel_cond_timedwait(&s->cond, &s->mutex, 5000)) {
            s->gate_failed = 1;
            return 0;
        }
    }
    return 1;
}

static int
lifecycle_write(char *data, int size, void *context)
{
    struct lifecycle_state *s;
    int ok;

    (void)data;
    s = context;
    sixel_mutex_lock(&s->mutex);
    s->write_entered = 1;
    sixel_cond_broadcast(&s->cond);
    ok = lifecycle_wait(s, &s->open) && !s->fail_write;
    sixel_mutex_unlock(&s->mutex);
    return ok ? size : -1;
}

static void
lifecycle_waiting(void *context)
{
    struct lifecycle_state *s;

    s = context;
    sixel_mutex_lock(&s->mutex);
    s->wait_entered = 1;
    sixel_cond_broadcast(&s->cond);
    sixel_mutex_unlock(&s->mutex);
}

static void
lifecycle_done(sixel_playback_result_t const *result, void *context)
{
    struct lifecycle_state *s;

    s = context;
    s->bad_sequence |= result->sequence != (uint64_t)s->count + 1;
    if (s->count < 4) s->completed[s->count] = result->status;
    ++s->count;
}

static int
lifecycle_reserve(void *context)
{
    struct lifecycle_state *s;

    s = context;
    s->result = sixel_playback_reserve(s->playback, SIXEL_PLAYBACK_WAIT,
        2000, &s->ticket);
    return 0;
}

/* Track the source object's destructor separately from palette scratch.
 * The test drops its reference before cancellation; only playback owns it. */
static void *lifecycle_source;
static int lifecycle_source_freed;

static void
lifecycle_free(void *pointer)
{
    if (pointer == lifecycle_source) ++lifecycle_source_freed;
    free(pointer);
}
#endif

/* 0/1 cancel queued frames, 2 release, 3 complete, 4 fail, 5 drain timeout,
 * 6 synchronous reservations. Each caller owns exactly one observation. */
int
playback_lifecycle_test(int mode)
{
#if SIXEL_ENABLE_THREADS
    struct lifecycle_state s;
    struct sixel_playback_test_hooks hooks;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    sixel_playback_stats_t stats;
    sixel_playback_ticket_t held;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame, *source;
    sixel_allocator_t *allocator;
    sixel_thread_t waiter;
    unsigned char gray[12] = {0};
    unsigned char ui[3] = {255, 255, 255};
    int ok, started, initialized, i, observed;

    memset(&s, 0, sizeof(s));
    memset(&held, 0, sizeof(held));
    encoder = NULL;
    output = NULL;
    frame = source = NULL;
    allocator = NULL;
    ok = started = initialized = 0;
    lifecycle_source = NULL;
    lifecycle_source_freed = 0;
    if (sixel_mutex_init(&s.mutex)) return 1;
    if (sixel_cond_init(&s.cond)) {
        sixel_mutex_destroy(&s.mutex);
        return 1;
    }
    initialized = 1;
    LIFE_CHECK(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    LIFE_CHECK(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADING_POLICY,
        mode == 6 ? "greedy:B1:prepare=serial"
                  : "greedy:B4:prepare=serial") == SIXEL_OK);
    LIFE_CHECK(sixel_encoder_setopt(encoder, 'p', "8") == SIXEL_OK);
    LIFE_CHECK(sixel_encoder_setopt(encoder, 'd', "none") == SIXEL_OK);
    if (mode == 1) {
        LIFE_CHECK(sixel_encoder_set_reserved_palette(encoder, ui, 1)
            == SIXEL_OK);
    }
    hooks.waiting = lifecycle_waiting;
    hooks.context = &s;
    encoder->playback_test_hooks = &hooks;
    LIFE_CHECK(sixel_output_new(&output, lifecycle_write, &s, NULL)
        == SIXEL_OK);
    LIFE_CHECK(sixel_playback_config_init(&config, sizeof(config))
        == SIXEL_OK);
    config.capacity = mode < 2 ? 4 : 1;
    config.done = lifecycle_done;
    config.done_context = &s;
    LIFE_CHECK(sixel_playback_new(&s.playback, encoder, output, &config,
        NULL) == SIXEL_OK);
    if (mode == 2 || mode == 6) {
        LIFE_CHECK(sixel_playback_reserve(s.playback, SIXEL_PLAYBACK_NOWAIT,
            0, &held) == SIXEL_OK);
    } else {
        frame = playback_test_frame(0);
        LIFE_CHECK(frame != NULL);
        LIFE_CHECK(sixel_playback_submit(s.playback, frame,
            SIXEL_PLAYBACK_NOWAIT, 0, NULL) == SIXEL_OK);
        sixel_frame_unref(frame);
        frame = NULL;
        sixel_mutex_lock(&s.mutex);
        observed = lifecycle_wait(&s, &s.write_entered);
        sixel_mutex_unlock(&s.mutex);
        LIFE_CHECK(observed);
    }
    if (mode < 2) {
        if (mode == 1) {
            LIFE_CHECK(sixel_allocator_new(&allocator, malloc, calloc,
                realloc, lifecycle_free) == SIXEL_OK);
            LIFE_CHECK(sixel_frame_new(&source, allocator) == SIXEL_OK);
            lifecycle_source = source;
            LIFE_CHECK(sixel_frame_init_borrowed(source, gray, 2, 2,
                SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
        }
        LIFE_CHECK(sixel_playback_options_init(&options, sizeof(options))
            == SIXEL_OK);
        options.palette_source = source;
        for (i = 1; i < 4; ++i) {
            frame = playback_test_frame(i);
            LIFE_CHECK(frame != NULL);
            LIFE_CHECK(sixel_playback_submit(s.playback, frame,
                SIXEL_PLAYBACK_NOWAIT, 0, &options) == SIXEL_OK);
            sixel_frame_unref(frame);
            frame = NULL;
        }
        sixel_frame_unref(source);
        source = NULL;
        LIFE_CHECK(!lifecycle_source_freed);
        LIFE_CHECK(sixel_playback_cancel(s.playback) == SIXEL_OK);
    } else if (mode == 6) {
        /* Only expiry with a held slot matters here, not a one-second
         * latency. Keep a positive deadline to exercise the wait path. */
        LIFE_CHECK(sixel_playback_reserve(s.playback, SIXEL_PLAYBACK_WAIT,
            1, &s.ticket) == SIXEL_TIMEOUT);
        sixel_playback_release_reservation(s.playback, &held);
        LIFE_CHECK(sixel_playback_reserve(s.playback, SIXEL_PLAYBACK_NOWAIT,
            0, &s.ticket) == SIXEL_OK);
    } else if (mode == 5) {
        LIFE_CHECK(sixel_playback_drain(s.playback, 0) == SIXEL_TIMEOUT);
    } else {
        LIFE_CHECK(sixel_thread_create(&waiter, lifecycle_reserve, &s) == 0);
        started = 1;
        sixel_mutex_lock(&s.mutex);
        observed = lifecycle_wait(&s, &s.wait_entered);
        sixel_mutex_unlock(&s.mutex);
        LIFE_CHECK(observed);
        if (mode == 2) {
            sixel_playback_release_reservation(s.playback, &held);
        }
    }
    sixel_mutex_lock(&s.mutex);
    s.fail_write = mode == 4;
    s.open = 1;
    sixel_cond_broadcast(&s.cond);
    sixel_mutex_unlock(&s.mutex);
    if (started) {
        sixel_thread_join(&waiter);
        started = 0;
        LIFE_CHECK(mode == 4 ? SIXEL_FAILED(s.result) &&
            s.result != SIXEL_TIMEOUT : s.result == SIXEL_OK);
        LIFE_CHECK(mode == 4 ? s.ticket.owner == NULL
            : s.ticket.owner == s.playback && s.ticket.id != 0);
    }
    sixel_playback_release_reservation(s.playback, &s.ticket);
    if (mode == 4) {
        LIFE_CHECK(sixel_playback_drain(s.playback, 2000) == s.result);
    } else {
        LIFE_CHECK(sixel_playback_drain(s.playback, 2000) == SIXEL_OK);
    }
    if (mode < 2) {
        LIFE_CHECK(s.count == 4 && !s.bad_sequence);
        LIFE_CHECK(s.completed[0] == SIXEL_OK);
        for (i = 1; i < 4; ++i) {
            LIFE_CHECK(s.completed[i] == SIXEL_INTERRUPTED);
        }
    }
    memset(&stats, 0, sizeof(stats));
    stats.struct_size = sizeof(stats);
    LIFE_CHECK(sixel_playback_get_stats(s.playback, &stats) == SIXEL_OK);
    LIFE_CHECK(stats.depth == 0 && stats.reserved == 0 &&
        stats.free_slots == config.capacity);
    ok = !s.gate_failed;
end:
    if (initialized) {
        sixel_mutex_lock(&s.mutex);
        s.open = 1;
        sixel_cond_broadcast(&s.cond);
        sixel_mutex_unlock(&s.mutex);
    }
    if (s.playback != NULL) {
        sixel_playback_release_reservation(s.playback, &held);
        if (started) sixel_thread_join(&waiter);
        sixel_playback_release_reservation(s.playback, &s.ticket);
        sixel_playback_unref(s.playback);
    }
    sixel_frame_unref(frame);
    sixel_frame_unref(source);
    if (mode == 1 && lifecycle_source_freed != 1) ok = 0;
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    sixel_cond_destroy(&s.cond);
    sixel_mutex_destroy(&s.mutex);
    return ok ? 0 : 1;
#else
    (void)mode;
    return 77;
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
