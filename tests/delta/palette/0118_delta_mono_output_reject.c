/* SPDX-License-Identifier: MIT */
/* Verify mono output reject. */
int test_delta_mono_runtime(int);

int
test_delta_native_0205(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_mono_runtime(0);
}
