/* SPDX-License-Identifier: MIT */
#include "rendezvous.h"
#include "src/lookup-policy-gpu.h"
#include <stdlib.h>
#include <string.h>
#if SIXEL_ENABLE_THREADS
/* Called with the fixture lock. The deadline only catches broken gates. */
static int
lso2_gpu_rendezvous_wait_locked(struct lso2_gpu_rendezvous_fixture *s, int *value)
{
    while (!*value) {
        if (sixel_cond_timedwait(&s->cond, &s->mutex, 5000)) {
            s->gate_failed = 1;
            return 0;
        }
    }
    return 1;
}

int
lso2_gpu_rendezvous_wait(struct lso2_gpu_rendezvous_fixture *s, int *value)
{
    int ok;

    sixel_mutex_lock(&s->mutex);
    ok = lso2_gpu_rendezvous_wait_locked(s, value);
    sixel_mutex_unlock(&s->mutex);
    return ok;
}

void
lso2_gpu_rendezvous_release(struct lso2_gpu_rendezvous_fixture *s, int *gate)
{
    sixel_mutex_lock(&s->mutex);
    *gate = 1;
    sixel_cond_broadcast(&s->cond);
    sixel_mutex_unlock(&s->mutex);
}

static int
lso2_gpu_rendezvous_write(char *data, int size, void *context)
{
    struct lso2_gpu_rendezvous_fixture *s;
    int ok;

    s = context;
    sixel_mutex_lock(&s->mutex);
    s->writer_entered = 1;
    sixel_cond_broadcast(&s->cond);
    ok = lso2_gpu_rendezvous_wait_locked(s, &s->writer_open) && !s->fail_write;
    sixel_mutex_unlock(&s->mutex);
    return ok ? playback_test_write(data, size, &s->buffer) : -1;
}

static void
lso2_gpu_rendezvous_started(void *context, uint64_t sequence)
{
    struct lso2_gpu_rendezvous_fixture *s;

    s = context;
    sixel_mutex_lock(&s->mutex);
    ++s->started;
    s->bad_credit |= sequence > s->last_resolved + 1;
    sixel_mutex_unlock(&s->mutex);
}

static void
lso2_gpu_rendezvous_resolved(void *context, uint64_t sequence)
{
    struct lso2_gpu_rendezvous_fixture *s;

    s = context;
    sixel_mutex_lock(&s->mutex);
    s->last_resolved = sequence;
    if (sequence == 2) s->second_resolved = 1;
    sixel_cond_broadcast(&s->cond);
    sixel_mutex_unlock(&s->mutex);
}

static void
lso2_gpu_rendezvous_complete(void *context, uint64_t sequence)
{
    struct lso2_gpu_rendezvous_fixture *s;

    s = context;
    if (sequence != 2) return;
    sixel_mutex_lock(&s->mutex);
    s->prepared_status = SIXEL_OK;
    s->helper_complete = 1;
    sixel_cond_broadcast(&s->cond);
    (void)lso2_gpu_rendezvous_wait_locked(s, &s->helper_open);
    sixel_mutex_unlock(&s->mutex);
}

static void
lso2_gpu_rendezvous_resolving(void *context, uint64_t sequence)
{
    struct lso2_gpu_rendezvous_fixture *s;

    s = context;
    if (sequence != 1) return;
    sixel_mutex_lock(&s->mutex);
    s->resolve_entered = 1;
    sixel_cond_broadcast(&s->cond);
    if (s->hold_resolve)
        (void)lso2_gpu_rendezvous_wait_locked(s, &s->resolve_open);
    sixel_mutex_unlock(&s->mutex);
}

static void
lso2_gpu_rendezvous_encoding(void *context, uint64_t sequence)
{
    struct lso2_gpu_rendezvous_fixture *s;

    s = context;
    if (sequence != 1) return;
    sixel_mutex_lock(&s->mutex);
    s->encode_entered = 1;
    sixel_cond_broadcast(&s->cond);
    if (s->hold_encode)
        (void)lso2_gpu_rendezvous_wait_locked(s, &s->encode_open);
    sixel_mutex_unlock(&s->mutex);
}

static void
lso2_gpu_rendezvous_encoded(void *context, uint64_t sequence)
{
    struct lso2_gpu_rendezvous_fixture *s;

    s = context;
    if (sequence != 2) return;
    sixel_mutex_lock(&s->mutex);
    s->second_encoded = 1;
    sixel_cond_broadcast(&s->cond);
    sixel_mutex_unlock(&s->mutex);
}

static void
lso2_gpu_rendezvous_done(sixel_playback_result_t const *result, void *context)
{
    struct lso2_gpu_rendezvous_fixture *s;

    s = context;
    sixel_mutex_lock(&s->mutex);
    s->bad_sequence |= result->sequence != (uint64_t)s->count + 1;
    if (s->count < 4) s->completed[s->count] = result->status;
    if (s->count < 4) {
        s->samples[s->count].struct_size = sizeof(s->samples[s->count]);
        if (sixel_playback_get_stage_sample(s->playback, result->sequence,
                &s->samples[s->count]) != SIXEL_OK) s->bad_sequence = 1;
    }
    if (s->count < 4) {
        s->pipeline_samples[s->count].struct_size =
            sizeof(s->pipeline_samples[s->count]);
        if (sixel_playback_get_pipeline_sample(s->playback, result->sequence,
                &s->pipeline_samples[s->count]) != SIXEL_OK)
            s->bad_sequence = 1;
    }
    ++s->count;
    s->first_done = 1;
    sixel_cond_broadcast(&s->cond);
    sixel_mutex_unlock(&s->mutex);
}

static int
lso2_gpu_rendezvous_open_impl(struct lso2_gpu_rendezvous_fixture *s,
    char const *gpu, char const *prepare, char const *delta, int plane,
    char const *diffusion)
{
    sixel_playback_config_t config;
    char policy[80];

    memset(s, 0, sizeof(*s));
    s->helper_open = 1;
    if (sixel_mutex_init(&s->mutex)) return 1;
    if (sixel_cond_init(&s->cond)) {
        sixel_mutex_destroy(&s->mutex);
        return 1;
    }
    s->initialized = 1;
    AHEAD_CHECK(sixel_encoder_new(&s->encoder, NULL) == SIXEL_OK);
    (void)snprintf(policy, sizeof(policy),
        "greedy:B4:prepare=%s:prepare_workers=1", prepare);
    AHEAD_CHECK(sixel_encoder_setopt(s->encoder,
        SIXEL_OPTFLAG_THREADING_POLICY, policy) == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(s->encoder, 'G', gpu) == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(s->encoder, '~', "ordered") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(s->encoder, 'd', diffusion) == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(s->encoder, 'p', "16") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(s->encoder, 'Z', delta) == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_set_6delta_plane_size(s->encoder, plane, plane)
        == SIXEL_OK);
    s->hooks.completed = lso2_gpu_rendezvous_complete;
    s->hooks.started = lso2_gpu_rendezvous_started;
    s->hooks.resolved = lso2_gpu_rendezvous_resolved;
    s->hooks.resolving = lso2_gpu_rendezvous_resolving;
    s->hooks.encoding = lso2_gpu_rendezvous_encoding;
    s->hooks.encoded = lso2_gpu_rendezvous_encoded;
    s->hooks.context = s;
    s->encoder->solve_test_hooks = &s->hooks;
    AHEAD_CHECK(sixel_output_new(&s->output, lso2_gpu_rendezvous_write, s, NULL)
        == SIXEL_OK);
    AHEAD_CHECK(sixel_playback_config_init(&config, sizeof(config))
        == SIXEL_OK);
    config.capacity = 4;
    config.done = lso2_gpu_rendezvous_done;
    config.done_context = s;
    AHEAD_CHECK(sixel_playback_new(&s->playback, s->encoder, s->output,
        &config, NULL) == SIXEL_OK);
    return 0;
end:
    return 1;
}

int
lso2_gpu_rendezvous_submit_origin(struct lso2_gpu_rendezvous_fixture *s,
    int index, int origin)
{
    sixel_playback_options_t options;
    sixel_frame_t *frame;
    SIXELSTATUS status;

    if (sixel_playback_options_init(&options, sizeof(options)) != SIXEL_OK)
        return 0;
    if (origin >= 0) {
        options.flags |= SIXEL_PLAYBACK_OPTION_ORIGIN;
        options.origin_x = origin;
        options.origin_y = origin;
    }
    frame = playback_test_small_frame(index);
    if (frame == NULL) return 0;
    status = sixel_playback_submit(s->playback, frame,
        SIXEL_PLAYBACK_NOWAIT, 0, &options);
    sixel_frame_unref(frame);
    return status == SIXEL_OK;
}

void
lso2_gpu_rendezvous_close(struct lso2_gpu_rendezvous_fixture *s)
{
    if (s->initialized) {
        lso2_gpu_rendezvous_release(s, &s->writer_open);
        lso2_gpu_rendezvous_release(s, &s->helper_open);
        lso2_gpu_rendezvous_release(s, &s->resolve_open);
        lso2_gpu_rendezvous_release(s, &s->encode_open);
    }
    if (s->playback != NULL) sixel_playback_unref(s->playback);
    if (s->output != NULL) sixel_output_unref(s->output);
    if (s->encoder != NULL) sixel_encoder_unref(s->encoder);
    free(s->buffer.bytes);
    if (s->initialized) {
        sixel_cond_destroy(&s->cond);
        sixel_mutex_destroy(&s->mutex);
    }
}

int
lso2_gpu_rendezvous_open(struct lso2_gpu_rendezvous_fixture *s, char const *gpu,
    char const *prepare, char const *delta)
{
    return lso2_gpu_rendezvous_open_impl(s, gpu, prepare, delta, 0, "lso2");
}

/* Auto exercises the independent controller even without a GPU; off selects
 * legacy scheduling. Ordinary indexed output needs no deferred GPU solve. */
int
lso2_gpu_rendezvous_open_portable(struct lso2_gpu_rendezvous_fixture *s,
    char const *gpu)
{
    return lso2_gpu_rendezvous_open_impl(s, gpu, "ahead", "delta:T0", 0,
        "none");
}

int
lso2_gpu_rendezvous_open_rect(struct lso2_gpu_rendezvous_fixture *s)
{
    return lso2_gpu_rendezvous_open_impl(s, "force", "ahead", "delta:T0", 64,
        "lso2");
}

int
lso2_gpu_rendezvous_submit(struct lso2_gpu_rendezvous_fixture *s, int index)
{
    return lso2_gpu_rendezvous_submit_origin(s, index, -1);
}
int
lso2_gpu_rendezvous_device(void)
{
    sixel_gpu_device_interface_t *device;
    SIXELSTATUS status;
    int outcome;

    device = NULL;
    status = sixel_gpu_device_acquire(&device);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    if (status != SIXEL_OK) return 1;
    outcome = sixel_gpu_lookup_has_program(SIXEL_LUT_POLICY_ORDERED,
        device->vtbl->program_format(device)) ? 0 : 77;
    device->vtbl->unref(device);
    return outcome;
}
#endif
