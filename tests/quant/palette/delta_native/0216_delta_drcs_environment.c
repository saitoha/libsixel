/* SPDX-License-Identifier: MIT */
/* Verify drcs environment. */
int test_delta_drcs_options(int);

int
test_delta_native_0216(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_drcs_options(2);
}
