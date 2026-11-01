/* Reject a TGA-like header whose palette flag conflicts with image type. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0173_loader_builtin_tga_header_collision_reject(int argc,
                                                             char **argv)
{
    static unsigned char const tga_like[] = {
        0x00u, 0x01u, 0x02u, 0x00u, 0x00u, 0x01u, 0x00u, 0x18u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x01u, 0x00u,
        0x18u, 0x20u, 0x00u, 0x00u, 0xffu
    };

    (void)argc;
    (void)argv;
    return edge_expect_failure("TGA header collision",
                               tga_like,
                               sizeof(tga_like));
}
