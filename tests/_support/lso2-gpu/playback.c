/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "tests/_support/planner/pipeline/playback_test_common.h"
#include "src/encoder.h"
#include <stdlib.h>

/* The frame sequence and refresh clock are fixed. Both paths construct the
 * palette from the same source; only scheduling differs. */
static int
lso2_gpu_playback_compare_impl(char const *method, char const *delta,
    unsigned int capacity, int resize, int interval_only)
{
    playback_test_buffer_t reference, candidate;
    sixel_encoder_t *a, *b;
    sixel_output_t *out_a, *out_b;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    sixel_playback_stage_sample_t sample;
    sixel_gpu_device_interface_t *device;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    int outcome, i;

    memset(&reference, 0, sizeof(reference));
    memset(&candidate, 0, sizeof(candidate));
    a = b = NULL;
    out_a = out_b = NULL;
    playback = NULL;
    device = NULL;
    frame = NULL;
    outcome = 1;
    status = sixel_gpu_device_acquire(&device);
    if (status == SIXEL_FEATURE_ERROR) { outcome = 77; goto end; }
    LSO2_GPU_CHECK(status == SIXEL_OK);
    a = playback_test_encoder(0, "4");
    b = playback_test_encoder(0, "4");
    LSO2_GPU_CHECK(a != NULL && b != NULL);
    LSO2_GPU_CHECK(sixel_encoder_setopt(a, SIXEL_OPTFLAG_DIFFUSION, method)
        == SIXEL_OK);
    LSO2_GPU_CHECK(sixel_encoder_setopt(b, SIXEL_OPTFLAG_DIFFUSION, method)
        == SIXEL_OK);
    LSO2_GPU_CHECK(sixel_encoder_setopt(a, SIXEL_OPTFLAG_GPU_POLICY, "force")
        == SIXEL_OK);
    LSO2_GPU_CHECK(sixel_encoder_setopt(b, SIXEL_OPTFLAG_GPU_POLICY, "force")
        == SIXEL_OK);
    if (delta != NULL) {
        LSO2_GPU_CHECK(sixel_encoder_setopt(a, SIXEL_OPTFLAG_UPDATE_POLICY,
            delta) == SIXEL_OK);
        LSO2_GPU_CHECK(sixel_encoder_setopt(b, SIXEL_OPTFLAG_UPDATE_POLICY,
            delta) == SIXEL_OK);
    }
    LSO2_GPU_CHECK(sixel_output_new(&out_a, playback_test_write, &reference,
        NULL) == SIXEL_OK);
    LSO2_GPU_CHECK(sixel_output_new(&out_b, playback_test_write, &candidate,
        NULL) == SIXEL_OK);
    for (i = 0; i < 4; ++i) {
        frame = resize == 1 && i == 2 ? playback_test_frame(i)
            : playback_test_small_frame(i);
        LSO2_GPU_CHECK(frame != NULL);
        if (resize == 2 && i == 2)
            sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_G8);
        LSO2_GPU_CHECK(sixel_encoder_encode_frame(a, frame, out_a) == SIXEL_OK);
        sixel_frame_unref(frame);
        frame = NULL;
    }
    LSO2_GPU_CHECK(sixel_playback_config_init(&config, sizeof(config))
        == SIXEL_OK);
    config.capacity = capacity;
    LSO2_GPU_CHECK(sixel_playback_new(&playback, b, out_b, &config, NULL)
        == SIXEL_OK);
    for (i = 0; i < 4; ++i) {
        frame = resize == 1 && i == 2 ? playback_test_frame(i)
            : playback_test_small_frame(i);
        LSO2_GPU_CHECK(frame != NULL);
        if (resize == 2 && i == 2)
            sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_G8);
        LSO2_GPU_CHECK(sixel_playback_options_init(&options,
            sizeof(options)) == SIXEL_OK);
        LSO2_GPU_CHECK(sixel_playback_submit(playback, frame,
            SIXEL_PLAYBACK_WAIT, -1, &options) == SIXEL_OK);
        sixel_frame_unref(frame);
        frame = NULL;
    }
    LSO2_GPU_CHECK(sixel_playback_drain(playback, 5000) == SIXEL_OK);
    memset(&sample, 0, sizeof(sample));
    sample.struct_size = sizeof(sample);
    LSO2_GPU_CHECK(sixel_playback_get_stage_sample(playback, 4, &sample)
        == SIXEL_OK);
    /* Prove admission to the intended path before comparing its bytes. */
    LSO2_GPU_CHECK(!!(sample.valid & SIXEL_PLAYBACK_STAGE_DEFERRED)
        == (capacity > 1 && resize != 2));
    if (interval_only) {
        LSO2_GPU_CHECK(!(sample.valid &
            SIXEL_PLAYBACK_STAGE_COMPLETION_INTERVAL));
        outcome = 0;
        goto end;
    }
    LSO2_GPU_CHECK(reference.size == candidate.size);
    LSO2_GPU_CHECK(memcmp(reference.bytes, candidate.bytes, reference.size)
        == 0);
    outcome = 0;
end:
    if (device != NULL) device->vtbl->unref(device);
    sixel_frame_unref(frame);
    if (playback != NULL) sixel_playback_unref(playback);
    sixel_encoder_unref(a);
    sixel_encoder_unref(b);
    sixel_output_unref(out_a);
    sixel_output_unref(out_b);
    free(reference.bytes);
    free(candidate.bytes);
    return outcome;
}

int
lso2_gpu_playback_compare(char const *method, char const *delta,
    unsigned int capacity)
{
    return lso2_gpu_playback_compare_impl(method, delta, capacity, 0, 0);
}

int
lso2_gpu_playback_resize_compare(int interval_only)
{
    return lso2_gpu_playback_compare_impl("lso2", "delta:T0", 4, 1,
        interval_only);
}

/* A non-native frame must drain the independent prefix and return to the
 * existing controller without losing the prefix or replaying callbacks. */
int
lso2_gpu_playback_format_transition(void)
{
    return lso2_gpu_playback_compare_impl("lso2", NULL, 4, 2, 0);
}
