/* Verify that a late APNG sequence gap cannot fall back to static. */

#include <string.h>

#include "loader_builtin_memory_test_common.h"

int
test_loader_0181_apng_late_sequence_reject(
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
        "APNG late sequence gap",
        "/tests/data/inputs/formats/"
        "apng_invalid_libpng_fctl_sequence_gap.png",
        0,
        &probe,
        &status);
    if (result != 0 || status != SIXEL_BAD_INPUT ||
        probe.callback_count != 1) {
        return 1;
    }
    return 0;
}
