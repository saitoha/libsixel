/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "fixture.h"
#include "src/encoder.h"
#include "tests/_support/planner/pipeline/playback_test_common.h"
#include <stdlib.h>

#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED
struct phased_latest_trace {
    sixel_playback_t *playback;
    uint64_t selected[8];
    int completed, failed;
};

static void
phased_latest_done(sixel_playback_result_t const *result, void *context)
{
    struct phased_latest_trace *trace;
    sixel_playback_pipeline_sample_t sample;

    trace = context;
    memset(&sample, 0, sizeof(sample));
    sample.struct_size = sizeof(sample);
    if (trace->completed >= 8 || result->sequence !=
            (uint64_t)trace->completed + 1 || result->status != SIXEL_OK) {
        trace->failed = 1;
        return;
    }
    trace->selected[trace->completed++] = result->palette_sequence;
    if (!result->palette_sequence || result->palette_sequence > result->sequence
            || sixel_playback_get_pipeline_sample(trace->playback,
                result->sequence, &sample) != SIXEL_OK ||
            !(sample.valid & SIXEL_PLAYBACK_PIPELINE_INDEPENDENT))
        trace->failed = 1;
}

#endif

/* Replay the observed palette-source sequence with a serial, per-frame
 * player. This compares the same palettes under different scheduling,
 * instead of assuming two asynchronous latest runs choose equal snapshots.
 * Eight admissions also reuse every descriptor/packet slot. */
int
phased_latest_replay(char const *method, char const *delta)
{
#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED
    playback_test_buffer_t candidate, reference;
    struct phased_latest_trace trace;
    sixel_encoder_t *a, *b;
    sixel_output_t *out_a, *out_b;
    sixel_playback_t *serial;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    sixel_gpu_device_interface_t *device;
    sixel_frame_t *frame, *source;
    SIXELSTATUS status;
    int i, result;

    memset(&candidate, 0, sizeof(candidate));
    memset(&reference, 0, sizeof(reference));
    memset(&trace, 0, sizeof(trace));
    a = b = NULL;
    out_a = out_b = NULL;
    serial = NULL;
    frame = source = NULL;
    device = NULL;
    result = 1;
    status = sixel_gpu_device_acquire(&device);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    PHASED_CHECK(status == SIXEL_OK);
    a = playback_test_encoder(0, "4");
    b = playback_test_encoder(0, "4");
    PHASED_CHECK(a && b);
    PHASED_CHECK(sixel_encoder_setopt(a, 'd', method) == SIXEL_OK);
    PHASED_CHECK(sixel_encoder_setopt(b, 'd', method) == SIXEL_OK);
    PHASED_CHECK(sixel_encoder_setopt(a, 'G', "force") == SIXEL_OK);
    PHASED_CHECK(sixel_encoder_setopt(b, 'G', "force") == SIXEL_OK);
    PHASED_CHECK(sixel_encoder_setopt(a, 'Z', delta) == SIXEL_OK);
    PHASED_CHECK(sixel_encoder_setopt(b, 'Z', delta) == SIXEL_OK);
    PHASED_CHECK(sixel_output_new(&out_a, playback_test_write, &candidate,
        NULL) == SIXEL_OK);
    PHASED_CHECK(sixel_output_new(&out_b, playback_test_write, &reference,
        NULL) == SIXEL_OK);
    (void)sixel_playback_config_init(&config, sizeof(config));
    config.capacity = 4;
    config.palette_mode = SIXEL_PLAYBACK_PALETTE_LATEST;
    config.done = phased_latest_done;
    config.done_context = &trace;
    PHASED_CHECK(sixel_playback_new(&trace.playback, a, out_a, &config, NULL)
        == SIXEL_OK);
    for (i = 0; i < 8; ++i) {
        frame = playback_test_small_frame(i);
        PHASED_CHECK(frame != NULL);
        (void)sixel_playback_options_init(&options, sizeof(options));
        PHASED_CHECK(sixel_playback_submit(trace.playback, frame,
            SIXEL_PLAYBACK_WAIT, 5000, &options) == SIXEL_OK);
        sixel_frame_unref(frame);
        frame = NULL;
    }
    PHASED_CHECK(sixel_playback_drain(trace.playback, 5000) == SIXEL_OK);
    PHASED_CHECK(trace.completed == 8 && !trace.failed);
    (void)sixel_playback_config_init(&config, sizeof(config));
    config.capacity = 1;
    PHASED_CHECK(sixel_playback_new(&serial, b, out_b, &config, NULL)
        == SIXEL_OK);
    for (i = 0; i < 8; ++i) {
        frame = playback_test_small_frame(i);
        source = playback_test_small_frame((int)trace.selected[i] - 1);
        PHASED_CHECK(frame && source);
        (void)sixel_playback_options_init(&options, sizeof(options));
        options.palette_source = source;
        PHASED_CHECK(sixel_playback_submit(serial, frame,
            SIXEL_PLAYBACK_WAIT, 5000, &options) == SIXEL_OK);
        sixel_frame_unref(frame);
        sixel_frame_unref(source);
        frame = source = NULL;
    }
    PHASED_CHECK(sixel_playback_drain(serial, 5000) == SIXEL_OK);
    PHASED_CHECK(candidate.size == reference.size);
    PHASED_CHECK(!memcmp(candidate.bytes, reference.bytes, candidate.size));
    result = 0;
end:
    sixel_playback_unref(trace.playback);
    sixel_playback_unref(serial);
    sixel_frame_unref(frame);
    sixel_frame_unref(source);
    sixel_encoder_unref(a);
    sixel_encoder_unref(b);
    sixel_output_unref(out_a);
    sixel_output_unref(out_b);
    if (device) device->vtbl->unref(device);
    free(candidate.bytes);
    free(reference.bytes);
    return result;
#else
    (void)method;
    (void)delta;
    return 77;
#endif
}
