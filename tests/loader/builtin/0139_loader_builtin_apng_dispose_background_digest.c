/* Fix the canvases around APNG BACKGROUND disposal. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0139_loader_builtin_apng_dispose_background_digest(
    int argc,
    char **argv)
{
    uint64_t const expected[2] = {
        UINT64_C(0xb6a23e02628ad0e5), UINT64_C(0xa428a0e894045e75)
    };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "APNG dispose BACKGROUND",
        "/tests/data/inputs/formats/apng_8x8_dispose_background.png",
        0,
        8,
        8,
        2,
        expected);
}
