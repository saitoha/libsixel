/* SPDX-License-Identifier: MIT */
#include "tests/_support/planner/pipeline/gpu_ahead_common.h"

int
test_gpu_ahead_explicit_serial(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    struct gpu_ahead_fixture s;
    int ok;

    ok = 0;
    (void)argc;
    (void)argv;
    AHEAD_CHECK(!gpu_ahead_open(&s, "auto:P1000000", "serial", "delta:T0"));
    AHEAD_CHECK(gpu_ahead_submit(&s, 0));
    AHEAD_CHECK(gpu_ahead_wait(&s, &s.writer_entered));
    AHEAD_CHECK(gpu_ahead_submit(&s, 1));
    gpu_ahead_release(&s, &s.writer_open);
    AHEAD_CHECK(sixel_playback_drain(s.playback, 5000) == SIXEL_OK);
    AHEAD_CHECK(s.count == 2 && !s.helper_entered);
    ok = 1;
end:
    gpu_ahead_close(&s);
    return !ok;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
