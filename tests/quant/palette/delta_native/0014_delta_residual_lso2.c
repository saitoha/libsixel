/* SPDX-License-Identifier: MIT */
/* Verify delta residual lso2. */
int test_delta_native_residual(int);

int
test_delta_native_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_native_residual(8);
}
