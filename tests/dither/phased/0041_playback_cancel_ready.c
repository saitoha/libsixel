/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/rendezvous.h"

int
test_phased_playback_cancel_ready(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED
    struct phased_rendezvous_fixture s;
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
    AHEAD_CHECK(!phased_rendezvous_open(&s, "force", "ahead", "delta:T0"));
    AHEAD_CHECK(phased_rendezvous_submit(&s, 0));
    AHEAD_CHECK(phased_rendezvous_wait(&s, &s.writer_entered));
    AHEAD_CHECK(phased_rendezvous_submit(&s, 1));
    AHEAD_CHECK(phased_rendezvous_wait(&s, &s.helper_complete));
    AHEAD_CHECK(sixel_playback_cancel(s.playback) == SIXEL_OK);
    phased_rendezvous_release(&s, &s.writer_open);
    AHEAD_CHECK(sixel_playback_drain(s.playback, 5000) == SIXEL_OK);
    AHEAD_CHECK(s.count == 2 && s.completed[1] == SIXEL_INTERRUPTED);
    ok = 1;
end:
    phased_rendezvous_close(&s);
    return !ok;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
