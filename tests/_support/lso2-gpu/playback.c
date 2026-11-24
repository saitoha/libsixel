/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "tests/_support/planner/pipeline/playback_test_common.h"
#include "src/encoder.h"
#include "src/lookup-policy-gpu.h"
#include <stdlib.h>

/* Preserve both complete SIXEL streams in the failing TAP transcript.
 * Hex keeps terminal controls inert and can be decoded for pixel comparison. */
static void
lso2_gpu_playback_dump(char const *name, playback_test_buffer_t const *buf)
{
    size_t i;

    for (i = 0; i < buf->size; ++i) {
        if (i % 32 == 0) fprintf(stderr, "%s[%zu] ", name, i);
        fprintf(stderr, "%02x", (unsigned int)buf->bytes[i]);
        if (i % 32 == 31 || i + 1 == buf->size) fputc('\n', stderr);
    }
}

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
    size_t ends[4], offset;
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
    /* The encoder selects the ordered lookup for GPU diffusion. */
    if (!sixel_gpu_lookup_has_program(SIXEL_LUT_POLICY_ORDERED,
            device->vtbl->program_format(device))) {
        outcome = 77;
        goto end;
    }
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
        ends[i] = reference.size;
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
    /* A failed helper elsewhere can leave an unrelated last-error string.
     * Report the first differing byte against the synchronous frame ends. */
    for (offset = 0; offset < reference.size && offset < candidate.size;
            ++offset) {
        if (reference.bytes[offset] != candidate.bytes[offset]) break;
    }
    if (offset != reference.size || offset != candidate.size) {
        fprintf(stderr, "playback %s %s capacity=%u stage=0x%x: "
            "sync=%zu queued=%zu sync frame ends=%zu,%zu,%zu,%zu; "
            "first difference at %zu (sync=%d queued=%d, -1=EOF)\n",
            method, delta != NULL ? delta : "full", capacity, sample.valid,
            reference.size, candidate.size, ends[0], ends[1], ends[2],
            ends[3], offset,
            offset < reference.size ? reference.bytes[offset] : -1,
            offset < candidate.size ? candidate.bytes[offset] : -1);
        lso2_gpu_playback_dump("sync", &reference);
        lso2_gpu_playback_dump("queued", &candidate);
        goto end;
    }
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
