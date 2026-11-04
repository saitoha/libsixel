/* SPDX-License-Identifier: MIT */
/* Verify serial node failure retry. */
int test_band_nodes_resources(int, int);

int
test_band_nodes_0062(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_band_nodes_resources(4, 1);
}
