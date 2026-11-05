/* SPDX-License-Identifier: MIT */
#include "tests/_support/planner/pipeline/gpu_ahead_common.h"

int
test_gpu_ahead_overlap(int argc, char **argv)
{
#if SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED
    struct gpu_ahead_fixture s;
    int ok;

    ok = 0;
    (void)argc;
    (void)argv;
    AHEAD_CHECK(!gpu_ahead_open(&s, "force", "ahead", "delta:T0"));
    AHEAD_CHECK(gpu_ahead_submit(&s, 0));
    AHEAD_CHECK(gpu_ahead_wait(&s, &s.writer_entered));
    AHEAD_CHECK(gpu_ahead_submit(&s, 1));
    AHEAD_CHECK(gpu_ahead_wait(&s, &s.helper_complete));
    AHEAD_CHECK(s.prepared_status == SIXEL_OK && s.count == 0);
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
