/* SPDX-License-Identifier: MIT */
#include "tests/_support/planner/pipeline/gpu_ahead_common.h"

int
test_prepare_ahead_write_failure_drain(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    struct gpu_ahead_fixture s;
    int ok;

    ok = 0;
    (void)argc;
    (void)argv;
    AHEAD_CHECK(!gpu_ahead_open(&s, "off", "ahead", "delta:T0"));
    s.helper_open = 0;
    s.fail_write = 1;
    AHEAD_CHECK(gpu_ahead_submit(&s, 0));
    AHEAD_CHECK(gpu_ahead_wait(&s, &s.writer_entered));
    AHEAD_CHECK(gpu_ahead_submit(&s, 1));
    AHEAD_CHECK(gpu_ahead_wait(&s, &s.helper_entered));
    gpu_ahead_release(&s, &s.writer_open);
    AHEAD_CHECK(gpu_ahead_wait(&s, &s.first_done));
    /* The next completion cannot release source ownership before join. */
    AHEAD_CHECK(s.count == 1);
    gpu_ahead_release(&s, &s.helper_open);
    AHEAD_CHECK(SIXEL_FAILED(sixel_playback_drain(s.playback, 5000)));
    AHEAD_CHECK(s.count == 2 && !s.bad_sequence &&
        s.completed[1] == SIXEL_INTERRUPTED);
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
