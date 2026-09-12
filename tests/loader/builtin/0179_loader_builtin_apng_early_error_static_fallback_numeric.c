/* Verify bounded static fallback for malformed APNG before raster decode. */

#include <string.h>

#include "loader_builtin_memory_test_common.h"

int
test_loader_0179_apng_early_static_fallback(
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
        "APNG early structural fallback",
        "/tests/data/inputs/formats/apng_invalid_num_frames_zero.png",
        0,
        &probe,
        &status);
    if (result != 0 || status != SIXEL_OK ||
        probe.callback_count != 1 || probe.width[0] != 8 ||
        probe.height[0] != 8) {
        return 1;
    }
    return 0;
}
