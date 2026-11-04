/* SPDX-License-Identifier: MIT */
#include "gpu_ahead_common.h"
#include <stdlib.h>
#include <string.h>
#if SIXEL_ENABLE_THREADS
/* Called with the fixture lock. The deadline only catches broken gates. */
static int
gpu_ahead_wait_locked(struct gpu_ahead_fixture *s, int *value)
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
gpu_ahead_wait(struct gpu_ahead_fixture *s, int *value)
{
    int ok;

    sixel_mutex_lock(&s->mutex);
    ok = gpu_ahead_wait_locked(s, value);
    sixel_mutex_unlock(&s->mutex);
    return ok;
}

void
gpu_ahead_release(struct gpu_ahead_fixture *s, int *gate)
{
    sixel_mutex_lock(&s->mutex);
    *gate = 1;
    sixel_cond_broadcast(&s->cond);
    sixel_mutex_unlock(&s->mutex);
}

static int
gpu_ahead_write(char *data, int size, void *context)
{
    struct gpu_ahead_fixture *s;
    int ok;

    s = context;
    sixel_mutex_lock(&s->mutex);
    s->writer_entered = 1;
    sixel_cond_broadcast(&s->cond);
    ok = gpu_ahead_wait_locked(s, &s->writer_open) && !s->fail_write;
    sixel_mutex_unlock(&s->mutex);
    return ok ? playback_test_write(data, size, &s->buffer) : -1;
}

static void
gpu_ahead_begin(void *context, uint64_t sequence)
{
    struct gpu_ahead_fixture *s;

    s = context;
    if (sequence != 2) return;
    sixel_mutex_lock(&s->mutex);
    s->helper_entered = 1;
    sixel_cond_broadcast(&s->cond);
    (void)gpu_ahead_wait_locked(s, &s->helper_open);
    sixel_mutex_unlock(&s->mutex);
}

static void
gpu_ahead_complete(void *context, uint64_t sequence, SIXELSTATUS status)
{
    struct gpu_ahead_fixture *s;

    s = context;
    if (sequence != 2) return;
    sixel_mutex_lock(&s->mutex);
    s->prepared_status = status;
    s->helper_complete = 1;
    sixel_cond_broadcast(&s->cond);
    sixel_mutex_unlock(&s->mutex);
}

static void
gpu_ahead_done(sixel_playback_result_t const *result, void *context)
{
    struct gpu_ahead_fixture *s;

    s = context;
    sixel_mutex_lock(&s->mutex);
    s->bad_sequence |= result->sequence != (uint64_t)s->count + 1;
    if (s->count < 4) s->completed[s->count] = result->status;
    ++s->count;
    s->first_done = 1;
    sixel_cond_broadcast(&s->cond);
    sixel_mutex_unlock(&s->mutex);
}

int
gpu_ahead_open(struct gpu_ahead_fixture *s, char const *gpu,
               char const *prepare, char const *delta)
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
    AHEAD_CHECK(sixel_encoder_setopt(s->encoder, 'd', "bluenoise") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(s->encoder, 'p', "16") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(s->encoder, 'Z', delta) == SIXEL_OK);
    s->hooks.begin = gpu_ahead_begin;
    s->hooks.complete = gpu_ahead_complete;
    s->hooks.context = s;
    s->encoder->prepare_test_hooks = &s->hooks;
    AHEAD_CHECK(sixel_output_new(&s->output, gpu_ahead_write, s, NULL)
        == SIXEL_OK);
    AHEAD_CHECK(sixel_playback_config_init(&config, sizeof(config))
        == SIXEL_OK);
    config.capacity = 4;
    config.done = gpu_ahead_done;
    config.done_context = s;
    AHEAD_CHECK(sixel_playback_new(&s->playback, s->encoder, s->output,
        &config, NULL) == SIXEL_OK);
    return 0;
end:
    return 1;
}

int
gpu_ahead_submit(struct gpu_ahead_fixture *s, int index)
{
    sixel_playback_options_t options;
    sixel_frame_t *frame;
    SIXELSTATUS status;

    if (sixel_playback_options_init(&options, sizeof(options)) != SIXEL_OK)
        return 0;
    /* Time-dependent refresh must have the same input in both schedules. */
    options.flags = SIXEL_PLAYBACK_OPTION_REFRESH_TIME;
    options.refresh_time_us = (uint64_t)index * 16667;
    frame = playback_test_small_frame(index);
    if (frame == NULL) return 0;
    status = sixel_playback_submit(s->playback, frame,
        SIXEL_PLAYBACK_NOWAIT, 0, &options);
    sixel_frame_unref(frame);
    return status == SIXEL_OK;
}

void
gpu_ahead_close(struct gpu_ahead_fixture *s)
{
    if (s->initialized) {
        gpu_ahead_release(s, &s->writer_open);
        gpu_ahead_release(s, &s->helper_open);
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

/* One complete byte-stream comparison, with identical temporal inputs. */
int
gpu_ahead_compare(char const *delta)
{
    struct gpu_ahead_fixture serial, ahead;
    int ok;

    memset(&serial, 0, sizeof(serial));
    memset(&ahead, 0, sizeof(ahead));
    ok = 0;
    AHEAD_CHECK(!gpu_ahead_open(&serial, "force", "serial", delta));
    AHEAD_CHECK(gpu_ahead_submit(&serial, 0));
    AHEAD_CHECK(gpu_ahead_wait(&serial, &serial.writer_entered));
    AHEAD_CHECK(gpu_ahead_submit(&serial, 1));
    AHEAD_CHECK(gpu_ahead_submit(&serial, 2));
    gpu_ahead_release(&serial, &serial.writer_open);
    AHEAD_CHECK(sixel_playback_drain(serial.playback, 5000) == SIXEL_OK);
    AHEAD_CHECK(!gpu_ahead_open(&ahead, "force", "ahead", delta));
    AHEAD_CHECK(gpu_ahead_submit(&ahead, 0));
    AHEAD_CHECK(gpu_ahead_wait(&ahead, &ahead.writer_entered));
    AHEAD_CHECK(gpu_ahead_submit(&ahead, 1));
    AHEAD_CHECK(gpu_ahead_wait(&ahead, &ahead.helper_complete));
    AHEAD_CHECK(gpu_ahead_submit(&ahead, 2));
    gpu_ahead_release(&ahead, &ahead.writer_open);
    AHEAD_CHECK(sixel_playback_drain(ahead.playback, 5000) == SIXEL_OK);
    AHEAD_CHECK(serial.buffer.size == ahead.buffer.size);
    AHEAD_CHECK(!memcmp(serial.buffer.bytes, ahead.buffer.bytes,
        serial.buffer.size));
    ok = 1;
end:
    gpu_ahead_close(&ahead);
    gpu_ahead_close(&serial);
    return !ok;
}
#endif
