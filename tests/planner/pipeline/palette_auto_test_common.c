/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/encoder-palette-auto.h"
#include "src/encoder.h"
#include "playback_test_common.h"
#include "palette_auto_test_common.h"

#define AUTO_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "auto scenario %d line %d\n", scenario, __LINE__); \
        return EXIT_FAILURE; \
    } \
} while (0)

static void
auto_samples(sixel_palette_auto_t *state, int count, double interval,
    double wait)
{
    int i;

    for (i = 0; i < count; ++i) {
        sixel_palette_auto_observe(state, interval, wait);
    }
}

/* Feed one complete order-balanced probe. The literal twenty-frame length
 * is independent of the production constant so a policy drift is visible. */
static void
auto_probe(sixel_palette_auto_t *state, double incumbent, double challenger)
{
    int i;
    int chosen;

    chosen = state->chosen;
    for (i = 0; i < 20; ++i) {
        sixel_palette_auto_observe(state,
            state->effective == chosen ? incumbent : challenger, 10.0);
    }
}

/* Literal durations are observations, not sleeps. */
int
palette_auto_test(int scenario)
{
    sixel_palette_auto_t state;
    int i;
    int j;
    int chosen;
    int expected;
    int next;
    unsigned int probes;
    double interval;

    sixel_palette_auto_init(&state);
    AUTO_CHECK(!state.effective && !state.probe);
    if (scenario == 0 || scenario == 1) {
        AUTO_CHECK(sixel_palette_auto_wait(40.0, 20.0, 10.0) == 0.0);
        AUTO_CHECK(sixel_palette_auto_wait(10.0, 20.0, 30.0) == 0.0);
        AUTO_CHECK(sixel_palette_auto_wait(10.0, 40.0, 30.0) == 10.0);
        AUTO_CHECK(sixel_palette_auto_wait(30.0, 40.0, 10.0) == 10.0);
        for (i = 0; i < 200; ++i) {
            AUTO_CHECK(!sixel_palette_auto_boundary(&state, 0.0));
            sixel_palette_auto_observe(&state, 100.0,
                sixel_palette_auto_wait(40.0, 20.0, 10.0));
        }
        AUTO_CHECK(state.probes == 0 && !state.effective);
        return EXIT_SUCCESS;
    }
    if (scenario == 9 || scenario == 10) {
        for (chosen = 0; chosen < 2; ++chosen) {
            sixel_palette_auto_init(&state);
            auto_samples(&state, 4, 100.0, 1e30);
            AUTO_CHECK(!state.probe);
            auto_samples(&state, 1, 100.0, 1e30);
            auto_probe(&state, 100.0, chosen ? 70.0 : 110.0);
            AUTO_CHECK(state.chosen == chosen);
            AUTO_CHECK(state.reevaluate_interval == 40);
            expected = 40;
            for (i = 0; i < 5; ++i) {
                probes = state.probes;
                auto_samples(&state, expected - 1, 100.0, 10.0);
                AUTO_CHECK(!state.probe && state.probes == probes);
                auto_samples(&state, 1, 100.0, 10.0);
                AUTO_CHECK(state.probe && state.probes == probes + 1);
                auto_probe(&state, 100.0, 110.0);
                expected = expected < 320 ? expected * 2 : 320;
                AUTO_CHECK(state.chosen == chosen && !state.probe);
                AUTO_CHECK(state.reevaluate_interval == expected);
            }
            if (scenario == 10) {
                auto_samples(&state, 320, 100.0, 10.0);
                auto_probe(&state, 100.0, 70.0);
                AUTO_CHECK(state.chosen == !chosen);
                AUTO_CHECK(state.reevaluate_interval == 40);
            }
        }
        return EXIT_SUCCESS;
    }
    auto_samples(&state, 4, 100.0, 10.0);
    AUTO_CHECK(!state.effective && !state.probe);
    AUTO_CHECK(!sixel_palette_auto_boundary(&state, 10.0));
    sixel_palette_auto_observe(&state, 100.0, 10.0);
    AUTO_CHECK(state.probe && !state.effective && state.probes == 1);
    if (scenario == 2) {
        for (i = 0; i < 20; ++i) {
            AUTO_CHECK(state.effective == (i % 4 == 1 || i % 4 == 2));
            AUTO_CHECK(sixel_palette_auto_boundary(&state, 0.0) == (i == 19));
            next = sixel_palette_auto_next(&state);
            sixel_palette_auto_observe(&state,
                i == 2 ? 0.0 : state.effective ? 70.0 : 100.0, 10.0);
            AUTO_CHECK(i == 19 || state.effective == next);
        }
        AUTO_CHECK(!state.probe && !state.chosen && state.pairs == 4);
        AUTO_CHECK(!strcmp(state.decision, "uncertain"));
        AUTO_CHECK(state.reevaluate_interval == 40);
        return EXIT_SUCCESS;
    }
    if (scenario == 3 || scenario == 4) {
        auto_probe(&state, 100.0, scenario == 3 ? 70.0 : 110.0);
        AUTO_CHECK(!state.probe && state.chosen == (scenario == 3));
        AUTO_CHECK(state.switches == (unsigned int)(scenario == 3));
        return EXIT_SUCCESS;
    }
    if (scenario == 5 || scenario == 11) {
        /* A 2% difference is hysteresis; a stable 4% gain wins in either
         * direction, with the same relative rule and a reset cadence. */
        auto_probe(&state, 100.0, scenario == 5 ? 98.0 : 96.0);
        AUTO_CHECK(state.chosen == (scenario == 11));
        auto_samples(&state, 40, 100.0, 10.0);
        auto_probe(&state, 100.0, 96.0);
        AUTO_CHECK(state.chosen == (scenario == 5));
        auto_samples(&state, 40, 100.0, 10.0);
        auto_probe(&state, 100.0, 98.0);
        AUTO_CHECK(state.chosen == (scenario == 5));
        AUTO_CHECK(!strcmp(state.decision, "uncertain"));
        AUTO_CHECK(state.reevaluate_interval == 40);
        return EXIT_SUCCESS;
    }
    if (scenario == 6) {
        auto_probe(&state, 100.0, 110.0);
        auto_samples(&state, 39, 100.0, 10.0);
        AUTO_CHECK(!state.probe && state.probes == 1);
        auto_samples(&state, 1, 100.0, 10.0);
        AUTO_CHECK(state.probe && state.probes == 2);
        auto_probe(&state, 100.0, 110.0);
        AUTO_CHECK(state.reevaluate_interval == 80);
        auto_samples(&state, 80, 100.0, 0.0);
        AUTO_CHECK(!state.probe && state.probes == 2);
        AUTO_CHECK(state.reevaluate_interval == 160);
        auto_samples(&state, 160, 100.0, 0.0);
        AUTO_CHECK(state.reevaluate_interval == 320 && !state.probe);
        return EXIT_SUCCESS;
    }
    if (scenario == 7) {
        auto_probe(&state, 100.0, 70.0);
        auto_samples(&state, 39, 100.0, 0.0);
        AUTO_CHECK(!state.probe && state.effective);
        auto_samples(&state, 1, 100.0, 0.0);
        AUTO_CHECK(state.probe && state.effective);
        auto_probe(&state, 100.0, 70.0);
        AUTO_CHECK(!state.probe && !state.chosen && state.switches == 2);
        return EXIT_SUCCESS;
    }
    if (scenario == 8) {
        for (i = 0; i < 20; ++i) {
            interval = state.effective ? 70.0 : 100.0;
            sixel_palette_auto_observe(&state,
                i == 1 || i == 5 ? 10000.0 : interval, 10.0);
        }
        AUTO_CHECK(state.chosen && state.switches == 1);
        return EXIT_SUCCESS;
    }
    if (scenario == 12 || scenario == 13) {
        for (chosen = 0; chosen < 2; ++chosen) {
            sixel_palette_auto_init(&state);
            auto_samples(&state, 5, 100.0, 10.0);
            auto_probe(&state, 100.0, chosen ? 70.0 : 110.0);
            for (j = 0; j < 3; ++j) {
                auto_samples(&state, 40, 100.0, 10.0);
                for (i = 0; i < 20; ++i) {
                    /* Content gets much cheaper in the probe, including
                     * a single cut inside one pair. Identical modes must
                     * not be confused with that common content change. */
                    interval = i < 7 ? 60.0 : 30.0;
                    if (scenario == 13) {
                        /* About 6% median gain with 2% MAD: a 3% floor
                         * or one MAD would switch, but two MADs must not. */
                        interval = state.effective == chosen ? 100.0 :
                            90.0 + 2.0 * (i / 4);
                    }
                    sixel_palette_auto_observe(&state, interval, 10.0);
                }
                AUTO_CHECK(state.chosen == chosen && !state.probe);
                AUTO_CHECK(!strcmp(state.decision, "uncertain"));
                AUTO_CHECK(state.reevaluate_interval == 40);
            }
        }
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

#if SIXEL_ENABLE_THREADS
typedef struct auto_playback_state {
    playback_test_buffer_t buffer;
    int callbacks;
    int observations;
    int failed;
    int latest;
    int probes;
    int switches;
} auto_playback_state_t;

static void
auto_playback_wait(void *context, uint64_t sequence, double *wait)
{
    (void)context;
    (void)sequence;
    *wait = 0.01;
}

static void
auto_playback_interval(void *context, uint64_t sequence, int latest,
    double *interval)
{
    (void)context;
    *interval = latest ? 0.05 : sequence < 67 ? 0.1 : 0.02;
}

static void
auto_playback_observed(void *context, uint64_t sequence, int latest,
    int probe, double interval, double wait, char const *decision)
{
    auto_playback_state_t *state;
    int expected_latest;
    int expected_probe;

    state = context;
    (void)interval;
    (void)wait;
    ++state->observations;
    expected_probe = (sequence >= 7 && sequence <= 26) ||
        (sequence >= 67 && sequence <= 86);
    expected_latest = sequence >= 27 && sequence <= 66;
    if (sequence >= 7 && sequence <= 26) {
        expected_latest = (sequence - 7) % 4 == 1 ||
            (sequence - 7) % 4 == 2;
    } else if (sequence >= 67 && sequence <= 86) {
        expected_latest = (sequence - 67) % 4 == 0 ||
            (sequence - 67) % 4 == 3;
    }
    state->failed |= latest != expected_latest || probe != expected_probe;
    state->latest = latest;
    state->probes += !strncmp(decision, "probe-", 6);
    state->switches += !strncmp(decision, "switch-", 7);
}

static void
auto_playback_done(sixel_playback_result_t const *result, void *context)
{
    auto_playback_state_t *state;

    state = context;
    ++state->callbacks;
    state->failed |= result->status != SIXEL_OK ||
        result->sequence != (uint64_t)state->callbacks ||
        result->palette_sequence == 0 ||
        result->palette_sequence > result->sequence;
    if (!state->latest) {
        state->failed |= result->palette_sequence != result->sequence;
    }
}
#endif

int
palette_auto_playback_test(void)
{
#if SIXEL_ENABLE_THREADS
    auto_playback_state_t state;
    struct sixel_encoder_palette_test_hooks hooks;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    int ahead;
    int pass;
    int i;
    int failed;

    encoder = NULL;
    output = NULL;
    playback = NULL;
    failed = 1;
    memset(&state, 0, sizeof(state));
    memset(&hooks, 0, sizeof(hooks));
    hooks.context = &state;
    hooks.auto_wait = auto_playback_wait;
    hooks.auto_interval = auto_playback_interval;
    hooks.auto_result = auto_playback_observed;
    encoder = playback_test_encoder(1, "4");
    if (encoder == NULL || sixel_output_new(&output, playback_test_write,
            &state.buffer, NULL) != SIXEL_OK) {
        goto end;
    }
    encoder->palette_test_hooks = &hooks;
    for (ahead = 0; ahead < 2; ++ahead) {
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADING_POLICY,
            ahead ? "greedy:B4:prepare=ahead" : "greedy:B4:prepare=serial");
        if (status != SIXEL_OK) {
            goto end;
        }
        /* Reattach to the same object: every session must start in frame. */
        for (pass = 0; pass < 2; ++pass) {
            state.callbacks = state.observations = state.failed = 0;
            state.latest = state.probes = state.switches = 0;
            (void)sixel_playback_config_init(&config, sizeof(config));
            config.palette_mode = SIXEL_PLAYBACK_PALETTE_AUTO;
            config.capacity = 4;
            config.done = auto_playback_done;
            config.done_context = &state;
            status = sixel_playback_new(&playback, encoder, output,
                &config, NULL);
            if (status != SIXEL_OK) {
                goto end;
            }
            for (i = 0; i < 90; ++i) {
                frame = playback_test_frame(i % 3);
                if (frame == NULL) {
                    goto end;
                }
                status = sixel_playback_submit(playback, frame,
                    SIXEL_PLAYBACK_WAIT, -1, NULL);
                sixel_frame_unref(frame);
                if (status != SIXEL_OK) {
                    goto end;
                }
            }
            status = sixel_playback_drain(playback, -1);
            sixel_playback_unref(playback);
            playback = NULL;
            if (status != SIXEL_OK || state.failed || state.callbacks != 90 ||
                    state.observations != 90 || state.probes != 2 ||
                    state.switches != 2) {
                fprintf(stderr, "auto playback ahead=%d pass=%d "
                    "status=%x bad=%d callbacks=%d observed=%d P=%d S=%d\n",
                    ahead, pass, status, state.failed, state.callbacks,
                    state.observations, state.probes, state.switches);
                goto end;
            }
        }
    }
    failed = 0;
end:
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    free(state.buffer.bytes);
    return failed;
#else
    return EXIT_SUCCESS;
#endif
}

#if SIXEL_ENABLE_THREADS
typedef struct auto_store_state {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    sixel_encoder_t *encoder;
    playback_test_buffer_t buffer;
    sixel_palette_snapshot_t *retained;
    uint64_t epoch;
    int open;
    int selected;
    int joined_before_select;
    int profile_change;
    int return_sequence;
    int retained_sequence;
    int failed;
    int callbacks;
} auto_store_state_t;

static void
auto_store_before_publish(void *context)
{
    auto_store_state_t *state;

    state = context;
    sixel_mutex_lock(&state->mutex);
    while (!state->open) {
        /* Only a failure escape; the selection or join opens the barrier. */
        if (sixel_cond_timedwait(&state->cond, &state->mutex, 5000) != 0) {
            state->failed = 1;
            break;
        }
    }
    sixel_mutex_unlock(&state->mutex);
}

static void
auto_store_before_join(void *context)
{
    auto_store_state_t *state;

    state = context;
    sixel_mutex_lock(&state->mutex);
    state->joined_before_select |= !state->selected;
    state->open = 1;
    sixel_cond_broadcast(&state->cond);
    sixel_mutex_unlock(&state->mutex);
}

static void
auto_store_build(void *context, uint64_t sequence,
    sixel_palette_job_test_hooks_t *hooks)
{
    auto_store_state_t *state;

    state = context;
    if (sequence == (uint64_t)state->return_sequence) {
        hooks->context = context;
        hooks->before_publish = auto_store_before_publish;
        hooks->before_join = auto_store_before_join;
    }
}

static void
auto_store_before_select(void *context, uint64_t sequence,
    sixel_palette_store_job_t *own)
{
    auto_store_state_t *state;

    state = context;
    if (sequence == (uint64_t)state->retained_sequence) {
        state->failed |= sixel_palette_store_job_join(own, SIXEL_OK)
            != SIXEL_OK;
    }
}

static void
auto_store_selected(void *context, uint64_t sequence,
    sixel_palette_selection_t const *selection)
{
    auto_store_state_t *state;

    state = context;
    state->failed |= selection->sequence > sequence ||
        selection->published >= selection->selected;
    if (sequence == (uint64_t)state->retained_sequence) {
        state->retained = selection->snapshot;
        sixel_palette_snapshot_ref(state->retained);
        state->epoch = selection->epoch;
        state->failed |= selection->sequence !=
            (uint64_t)state->retained_sequence;
    }
    if (sequence != (uint64_t)state->return_sequence) {
        return;
    }
    sixel_mutex_lock(&state->mutex);
    state->failed |= state->selected ||
        state->joined_before_select != state->profile_change;
    if (state->profile_change) {
        state->failed |= selection->sequence !=
                (uint64_t)state->return_sequence ||
            selection->epoch == state->epoch ||
            selection->snapshot == state->retained;
    } else {
        /* Pointer identity proves the same immutable completed object was
         * kept, not rebuilt or substituted with the return frame's own. */
        state->failed |= selection->sequence !=
            (uint64_t)state->retained_sequence ||
            selection->epoch != state->epoch ||
            selection->snapshot != state->retained || state->open;
    }
    state->selected = 1;
    state->open = 1;
    sixel_cond_broadcast(&state->cond);
    sixel_mutex_unlock(&state->mutex);
}

static void
auto_store_interval(void *context, uint64_t sequence, int latest,
    double *interval)
{
    auto_store_state_t *state;

    state = context;
    *interval = latest ? 0.05 : 0.1;
    if (state->return_sequence == 68 && sequence <= 26) {
        /* Keep frame after the first probe, then retain its last completed
         * latest palette throughout forty chosen frame applications. */
        *interval = latest ? 0.1 : 0.05;
    }
}

/* Independent expected schedule for short and long store-retention gaps. */
static int
auto_store_expected_latest(auto_store_state_t const *state, uint64_t sequence)
{
    uint64_t start;

    if (sequence < 7) {
        return 0;
    }
    start = 7;
    if (sequence > 26) {
        if (state->return_sequence != 68) {
            return 1;
        }
        if (sequence < 67) {
            return 0;
        }
        start = 67;
    }
    return (sequence - start) % 4 == 1 || (sequence - start) % 4 == 2;
}

static void
auto_store_observed(void *context, uint64_t sequence, int latest,
    int probe, double interval, double wait, char const *decision)
{
    auto_store_state_t *state;

    state = context;
    (void)probe;
    (void)interval;
    (void)wait;
    (void)decision;
    state->failed |= latest != auto_store_expected_latest(state, sequence);
    if (sequence + 1 == (uint64_t)state->return_sequence &&
            state->profile_change) {
        /* No ahead job crosses this decision boundary. Model a changed
         * color configuration after the intervening frame applications. */
        ++state->encoder->configuration_revision;
    }
}

static void
auto_store_done(sixel_playback_result_t const *result, void *context)
{
    auto_store_state_t *state;

    state = context;
    ++state->callbacks;
    state->failed |= result->status != SIXEL_OK ||
        result->sequence != (uint64_t)state->callbacks ||
        result->palette_sequence == 0 ||
        result->palette_sequence > result->sequence;
    if (!auto_store_expected_latest(state, result->sequence)) {
        state->failed |= result->palette_sequence != result->sequence;
    }
    if (result->sequence == (uint64_t)state->return_sequence) {
        state->failed |= result->palette_sequence !=
            (uint64_t)(state->profile_change ? state->return_sequence :
                state->retained_sequence);
    }
}
#endif

int
palette_auto_store_test(int profile_change)
{
#if SIXEL_ENABLE_THREADS
    auto_store_state_t state;
    struct sixel_encoder_palette_test_hooks hooks;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    int ahead;
    int delta;
    int period;
    int count;
    int i;
    int failed;

    memset(&state, 0, sizeof(state));
    memset(&hooks, 0, sizeof(hooks));
    encoder = NULL;
    output = NULL;
    playback = NULL;
    failed = 1;
    state.profile_change = profile_change;
    if (sixel_mutex_init(&state.mutex) != 0) {
        return EXIT_FAILURE;
    }
    if (sixel_cond_init(&state.cond) != 0) {
        sixel_mutex_destroy(&state.mutex);
        return EXIT_FAILURE;
    }
    hooks.context = &state;
    hooks.auto_wait = auto_playback_wait;
    hooks.auto_interval = auto_store_interval;
    hooks.auto_result = auto_store_observed;
    hooks.build = auto_store_build;
    hooks.before_select = auto_store_before_select;
    hooks.selected = auto_store_selected;
    for (delta = 0; delta < 2; ++delta) {
        encoder = playback_test_encoder(delta, "4");
        if (encoder == NULL || sixel_output_new(&output, playback_test_write,
                &state.buffer, NULL) != SIXEL_OK) {
            goto end;
        }
        /* Store retention observes schedule, snapshot identity and epochs.
         * A crop keeps nonuniform colors and default lookup construction,
         * while avoiding full-size FS application on every history frame. */
        if (sixel_encoder_setopt(encoder, 'c', "16x16+0+0") != SIXEL_OK) {
            goto end;
        }
        state.encoder = encoder;
        encoder->palette_test_hooks = &hooks;
        for (period = profile_change ? 2 : 0;
                period < (profile_change ? 4 : 6); ++period) {
            ahead = period % 2;
            state.return_sequence = period < 2 ? 12 : period < 4 ? 27 : 68;
            state.retained_sequence = period < 2 ? 9 : 25;
            count = state.return_sequence + 3;
            status = sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_THREADING_POLICY,
                ahead ? "greedy:B4:prepare=ahead"
                      : "greedy:B4:prepare=serial");
            if (status != SIXEL_OK) {
                goto end;
            }
            state.callbacks = state.failed = state.selected = state.open = 0;
            state.joined_before_select = 0;
            (void)sixel_playback_config_init(&config, sizeof(config));
            config.palette_mode = SIXEL_PLAYBACK_PALETTE_AUTO;
            config.capacity = 4;
            config.done = auto_store_done;
            config.done_context = &state;
            status = sixel_playback_new(&playback, encoder, output,
                &config, NULL);
            if (status != SIXEL_OK) {
                goto end;
            }
            for (i = 0; i < count; ++i) {
                frame = playback_test_frame(i % 3);
                if (frame == NULL) {
                    goto end;
                }
                status = sixel_playback_submit(playback, frame,
                    SIXEL_PLAYBACK_WAIT, -1, NULL);
                sixel_frame_unref(frame);
                if (status != SIXEL_OK) {
                    goto end;
                }
            }
            status = sixel_playback_drain(playback, -1);
            sixel_playback_unref(playback);
            playback = NULL;
            if (status != SIXEL_OK || state.failed || !state.selected ||
                    state.callbacks != count) {
                fprintf(stderr, "auto store profile=%d ahead=%d delta=%d "
                    "status=%x bad=%d selected=%d callbacks=%d\n",
                    profile_change, ahead, delta, status, state.failed,
                    state.selected, state.callbacks);
                goto end;
            }
            sixel_palette_snapshot_unref(state.retained);
            state.retained = NULL;
        }
        sixel_output_unref(output);
        output = NULL;
        sixel_encoder_unref(encoder);
        encoder = NULL;
    }
    failed = 0;
end:
    sixel_mutex_lock(&state.mutex);
    state.open = 1;
    sixel_cond_broadcast(&state.cond);
    sixel_mutex_unlock(&state.mutex);
    sixel_playback_unref(playback);
    sixel_palette_snapshot_unref(state.retained);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_cond_destroy(&state.cond);
    sixel_mutex_destroy(&state.mutex);
    free(state.buffer.bytes);
    return failed;
#else
    (void)profile_change;
    return EXIT_SUCCESS;
#endif
}
