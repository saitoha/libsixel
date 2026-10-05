/* SPDX-License-Identifier: MIT */
/* Verify delta residual stucki. */
int test_delta_native_residual(int);

int
test_delta_native_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_native_residual(3);
}
