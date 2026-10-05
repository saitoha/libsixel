/* SPDX-License-Identifier: MIT */
/* Verify delta signed threshold. */
int test_delta_native_threshold(int);

int
test_delta_native_0015(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_native_threshold(0);
}
