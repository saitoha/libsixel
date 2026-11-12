/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/rendezvous.h"

int
test_lso2_gpu_playback_failure_drain(int argc, char **argv)
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
    AHEAD_CHECK(lso2_gpu_rendezvous_submit(&s, 0));
    AHEAD_CHECK(lso2_gpu_rendezvous_wait(&s, &s.writer_entered));
    AHEAD_CHECK(lso2_gpu_rendezvous_submit(&s, 1));
    AHEAD_CHECK(lso2_gpu_rendezvous_wait(&s, &s.helper_complete));
    s.fail_write = 1;
    lso2_gpu_rendezvous_release(&s, &s.writer_open);
    AHEAD_CHECK(SIXEL_FAILED(sixel_playback_drain(s.playback, 5000)));
    AHEAD_CHECK(s.count == 2 && s.completed[1] == SIXEL_INTERRUPTED);
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
