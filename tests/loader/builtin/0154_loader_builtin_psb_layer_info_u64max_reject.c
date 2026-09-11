/* Verify that an unbounded PSB layer-info length is rejected safely. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0154_loader_builtin_psb_layer_info_u64max_reject(int argc,
                                                              char **argv)
{
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    int result;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    result = edge_load_fixture(
        "PSB UINT64_MAX layer-info length",
        "/tests/data/inputs/formats/"
        "snake16_psb_rgb8_missing_composite_multilayer_"
        "layer_info_length_u64max.psd",
        1,
        &probe,
        &status);
    if (result != 0 || SIXEL_SUCCEEDED(status) || probe.callback_count != 0) {
        return 1;
    }
    return 0;
}
