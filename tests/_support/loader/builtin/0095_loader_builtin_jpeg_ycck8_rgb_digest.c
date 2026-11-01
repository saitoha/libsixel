/* Verify YCCK conversion with an exact decoded RGB digest. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0095_loader_builtin_jpeg_ycck8_rgb_digest(
    int argc,
    char **argv)
{
    static uint64_t const expected_digests[1] = {
        UINT64_C(0xf016b4382324b868)
    };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "JPEG YCCK RGB conversion",
        "/tests/data/inputs/formats/snake-jpeg-8bit-ycck-seq444.jpg",
        1,
        64,
        64,
        1,
        expected_digests);
}
