/* SPDX-License-Identifier: MIT */
/* Verify delta residual atkinson. */
int test_delta_native_residual(int);

int
test_delta_native_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_native_residual(1);
}
