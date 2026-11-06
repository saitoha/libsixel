/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/rendezvous.h"

int
test_phased_playback_resolve_past_stalled_encode(int argc, char **argv)
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
    s.hold_encode = 1;
    AHEAD_CHECK(phased_rendezvous_submit(&s, 0));
    AHEAD_CHECK(phased_rendezvous_wait(&s, &s.encode_entered));
    AHEAD_CHECK(phased_rendezvous_submit(&s, 1));
    AHEAD_CHECK(phased_rendezvous_wait(&s, &s.second_resolved));
    AHEAD_CHECK(s.count == 0 && s.last_resolved == 2);
    phased_rendezvous_release(&s, &s.encode_open);
    phased_rendezvous_release(&s, &s.writer_open);
    AHEAD_CHECK(sixel_playback_drain(s.playback, 5000) == SIXEL_OK);
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
