/* Fix representative PSD layer-effect reconstruction with an RGB digest. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0161_loader_builtin_psd_effect_digest(int argc, char **argv)
{
    static uint64_t const expected[1] = {
        UINT64_C(0x2ebbb46da369ef37)
    };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "PSD layer-effect reconstruction",
        "/tests/data/inputs/formats/"
        "snake16_rgb8_missing_composite_multilayer_layer_effects.psd",
        1,
        16,
        16,
        1,
        expected);
}
