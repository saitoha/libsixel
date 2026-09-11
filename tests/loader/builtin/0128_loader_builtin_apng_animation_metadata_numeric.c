/* Verify APNG zero-denominator timing and sequence metadata. */

#include <string.h>

#include "loader_builtin_memory_test_common.h"

int
test_loader_0128_loader_builtin_apng_animation_metadata_numeric(
    int argc,
    char **argv)
{
    edge_frame_probe_t probe;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    if (edge_load_fixture(
            "APNG animation metadata",
            "/tests/data/inputs/formats/"
            "apng_8x8_libpng_delay_den_zero.png",
            0,
            &probe,
            &status) != 0 ||
        SIXEL_FAILED(status)) {
        return 1;
    }
    if (probe.callback_count != 2 ||
        probe.delay[0] != 1 || probe.delay[1] != 1 ||
        probe.frame_no[0] != 0 || probe.frame_no[1] != 1 ||
        probe.loop_no[0] != 0 || probe.loop_no[1] != 0 ||
        probe.multiframe[0] == 0 || probe.multiframe[1] == 0) {
        return 1;
    }
    return 0;
}
