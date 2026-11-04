/* SPDX-License-Identifier: MIT */
/* Verify serial node failure release. */
int test_band_nodes_resources(int, int);

int
test_band_nodes_0060(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_band_nodes_resources(3, 1);
}
