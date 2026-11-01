/* SPDX-License-Identifier: MIT */
/* Verify refine source mask. */
int test_delta_refinement(int);

int
test_delta_native_0239(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_refinement(5);
}
