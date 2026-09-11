/* Fix numeric CMYK-to-RGB output for a non-YCCK Adobe stream. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0144_loader_builtin_jpeg_cmyk8_digest(int argc, char **argv)
{
    uint64_t const expected[1] = { UINT64_C(0xc7f7339ecf343ce4) };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "JPEG CMYK8",
        "/tests/data/inputs/formats/snake-jpeg-8bit-cmyk-seq444.jpg",
        1,
        64,
        64,
        1,
        expected);
}
