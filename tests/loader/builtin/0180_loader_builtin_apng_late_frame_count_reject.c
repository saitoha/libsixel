/* Verify that a late APNG frame-count mismatch cannot fall back to static. */

#include <string.h>

#include "loader_builtin_memory_test_common.h"

int
test_loader_0180_apng_late_count_reject(
    int argc,
    char **argv)
{
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    int result;

    (void)argc;
    (void)argv;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    result = edge_load_fixture(
        "APNG late frame-count mismatch",
        "/tests/data/inputs/formats/apng_invalid_num_frames_mismatch.png",
        0,
        &probe,
        &status);
    if (result != 0 || status != SIXEL_BAD_INPUT ||
        probe.callback_count != 2) {
        return 1;
    }
    return 0;
}
