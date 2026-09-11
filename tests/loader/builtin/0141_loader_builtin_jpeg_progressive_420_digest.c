/* Fix output after all scans of a representative progressive 4:2:0 JPEG. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0141_loader_builtin_jpeg_progressive_420_digest(int argc,
                                                             char **argv)
{
    uint64_t const expected[1] = { UINT64_C(0xade587659436a76f) };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "JPEG progressive 4:2:0",
        "/tests/data/inputs/formats/snake-jpeg-8bit-ycbcr-prog420.jpg",
        1,
        64,
        64,
        1,
        expected);
}
