/* SPDX-License-Identifier: MIT */
/* Verify parallel node drain release. */
int test_band_nodes_resources(int, int);

int
test_band_nodes_0058(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_band_nodes_resources(1, 2);
}
