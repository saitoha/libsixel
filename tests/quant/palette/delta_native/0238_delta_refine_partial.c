/* SPDX-License-Identifier: MIT */
/* Verify partially painted frames still request refinement. */
int test_delta_refinement(int);

int
test_delta_native_0238(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_refinement(4);
}
