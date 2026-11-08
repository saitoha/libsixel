/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/rendezvous.h"

int
test_lso2_gpu_playback_cancel_origin(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED
    struct lso2_gpu_rendezvous_fixture s;
    sixel_gpu_device_interface_t *device;
    SIXELSTATUS status;
    int ok;

    (void)argc;
    (void)argv;
    device = NULL;
    status = sixel_gpu_device_acquire(&device);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    if (status != SIXEL_OK) return 1;
    device->vtbl->unref(device);
    ok = 0;
    AHEAD_CHECK(!lso2_gpu_rendezvous_open_rect(&s));
    s.helper_open = 0;
    AHEAD_CHECK(lso2_gpu_rendezvous_submit(&s, 0));
    AHEAD_CHECK(lso2_gpu_rendezvous_wait(&s, &s.writer_entered));
    AHEAD_CHECK(lso2_gpu_rendezvous_submit_origin(&s, 1, 5));
    AHEAD_CHECK(lso2_gpu_rendezvous_wait(&s, &s.helper_complete));
    AHEAD_CHECK(sixel_playback_cancel(s.playback) == SIXEL_OK);
    AHEAD_CHECK(lso2_gpu_rendezvous_submit(&s, 2));
    lso2_gpu_rendezvous_release(&s, &s.helper_open);
    lso2_gpu_rendezvous_release(&s, &s.writer_open);
    AHEAD_CHECK(sixel_playback_drain(s.playback, 5000) == SIXEL_OK);
    AHEAD_CHECK(s.count == 3 && s.completed[1] == SIXEL_INTERRUPTED &&
        s.completed[2] == SIXEL_OK);
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
