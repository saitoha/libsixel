/* SPDX-License-Identifier: MIT */
/* Wire snapshots from cc8da767c, before constant-time node unlinking. */
#include <sixel.h>

int test_node_unlink_stream(int, unsigned long, unsigned long);

int
test_node_unlink_0043(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_node_unlink_stream(SIXEL_ENCODEPOLICY_AUTO,
        0xe0eb202eUL, 0x8d26026aUL);
}
