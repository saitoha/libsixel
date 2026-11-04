/* SPDX-License-Identifier: MIT */
/* Verify serial node failure status. */
int test_band_nodes_resources(int, int);

int
test_band_nodes_0059(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_band_nodes_resources(2, 1);
}
