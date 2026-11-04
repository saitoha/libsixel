/* SPDX-License-Identifier: MIT */
/* Verify parallel node writer release. */
int test_band_nodes_resources(int, int);

int
test_band_nodes_0063(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_band_nodes_resources(5, 2);
}
