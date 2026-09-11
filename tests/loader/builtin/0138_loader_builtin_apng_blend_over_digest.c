/* Fix the two full canvases emitted after APNG SOURCE/OVER composition. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0138_loader_builtin_apng_blend_over_digest(int argc,
                                                        char **argv)
{
    uint64_t const expected[2] = {
        UINT64_C(0xb6a23e02628ad0e5), UINT64_C(0xc8a49d9fb87d6fe5)
    };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "APNG blend OVER",
        "/tests/data/inputs/formats/apng_8x8_blend_over.png",
        0,
        8,
        8,
        2,
        expected);
}
