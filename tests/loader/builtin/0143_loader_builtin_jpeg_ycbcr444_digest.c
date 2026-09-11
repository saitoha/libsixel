/* Fix numeric YCbCr-to-RGB output without chroma subsampling. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0143_loader_builtin_jpeg_ycbcr444_digest(int argc, char **argv)
{
    uint64_t const expected[1] = { UINT64_C(0xc1fc94371fecb084) };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "JPEG YCbCr 4:4:4",
        "/tests/data/inputs/formats/snake-jpeg-8bit-ycbcr-seq444.jpg",
        1,
        64,
        64,
        1,
        expected);
}
