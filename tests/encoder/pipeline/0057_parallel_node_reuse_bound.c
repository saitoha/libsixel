/* SPDX-License-Identifier: MIT */
/* Verify parallel node reuse bound. */
int test_band_nodes_resources(int, int);

int
test_band_nodes_0057(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_band_nodes_resources(0, 2);
}
