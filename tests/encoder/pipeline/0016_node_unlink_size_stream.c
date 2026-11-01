/* SPDX-License-Identifier: MIT */
/* Wire snapshots from cc8da767c, before constant-time node unlinking. */
#include <sixel.h>

int test_node_unlink_stream(int, unsigned long, unsigned long);

int
test_node_unlink_0044(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_node_unlink_stream(SIXEL_ENCODEPOLICY_SIZE,
        0xd3268a4cUL, 0x3893172eUL);
}
