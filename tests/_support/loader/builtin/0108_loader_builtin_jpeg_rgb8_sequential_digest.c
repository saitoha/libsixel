/* Fix the builtin sequential RGB JPEG decoder's byte output. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0108_loader_builtin_jpeg_rgb8_sequential_digest(
    int argc,
    char **argv)
{
    uint64_t const digest[1] = { UINT64_C(0x1d0457f30eb115bf) };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "JPEG sequential RGB8",
        "/tests/data/inputs/formats/snake-jpeg-8bit-rgb-seq444.jpg",
        1,
        64,
        64,
        1,
        digest);
}
