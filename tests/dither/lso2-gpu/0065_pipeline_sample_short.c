/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/rendezvous.h"
#include <string.h>

int
test_lso2_gpu_pipeline_sample_short(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED
    struct lso2_gpu_rendezvous_fixture s;
    sixel_gpu_device_interface_t *device;
    SIXELSTATUS status;
    int ok;
    sixel_playback_pipeline_sample_t sample, saved;

    (void)argc;
    (void)argv;
    device = NULL;
    status = sixel_gpu_device_acquire(&device);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    if (status != SIXEL_OK) return 1;
    device->vtbl->unref(device);
    ok = 0;
    AHEAD_CHECK(!lso2_gpu_rendezvous_open(&s, "force", "ahead", "delta:T0"));
    AHEAD_CHECK(lso2_gpu_rendezvous_submit(&s, 0));
    AHEAD_CHECK(lso2_gpu_rendezvous_wait(&s, &s.writer_entered));
    AHEAD_CHECK(lso2_gpu_rendezvous_submit(&s, 1));
    AHEAD_CHECK(lso2_gpu_rendezvous_wait(&s, &s.helper_complete));
    lso2_gpu_rendezvous_release(&s, &s.writer_open);
    AHEAD_CHECK(sixel_playback_drain(s.playback, 5000) == SIXEL_OK);
    memcpy(&sample, &s.pipeline_samples[1], sizeof(sample));
    sample.struct_size--;
    memcpy(&saved, &sample, sizeof(saved));
    AHEAD_CHECK(sixel_playback_get_pipeline_sample(s.playback, 2, &sample)
        == SIXEL_BAD_ARGUMENT);
    AHEAD_CHECK(memcmp(&sample, &saved, sizeof(sample)) == 0);
    ok = 1;
end:
    lso2_gpu_rendezvous_close(&s);
    return !ok;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
