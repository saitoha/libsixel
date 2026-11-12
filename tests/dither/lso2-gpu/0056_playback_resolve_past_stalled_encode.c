/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/rendezvous.h"

int
test_lso2_gpu_playback_resolve_past_stalled_encode(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED
    struct lso2_gpu_rendezvous_fixture s;
    int status;
    int ok;

    (void)argc;
    (void)argv;
    status = lso2_gpu_rendezvous_device();
    if (status != 0) return status;
    ok = 0;
    AHEAD_CHECK(!lso2_gpu_rendezvous_open(&s, "force", "ahead", "delta:T0"));
    s.hold_encode = 1;
    AHEAD_CHECK(lso2_gpu_rendezvous_submit(&s, 0));
    AHEAD_CHECK(lso2_gpu_rendezvous_wait(&s, &s.encode_entered));
    AHEAD_CHECK(lso2_gpu_rendezvous_submit(&s, 1));
    AHEAD_CHECK(lso2_gpu_rendezvous_wait(&s, &s.second_resolved));
    AHEAD_CHECK(s.count == 0 && s.last_resolved == 2);
    lso2_gpu_rendezvous_release(&s, &s.encode_open);
    lso2_gpu_rendezvous_release(&s, &s.writer_open);
    AHEAD_CHECK(sixel_playback_drain(s.playback, 5000) == SIXEL_OK);
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
