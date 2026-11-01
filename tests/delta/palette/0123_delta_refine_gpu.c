/* SPDX-License-Identifier: MIT */
/* Verify refine gpu. */
int test_delta_refinement(int);

int
test_delta_native_0236(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_refinement(2);
}
