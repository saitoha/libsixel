/* SPDX-License-Identifier: MIT */
/* Verify or full override. */
int test_delta_or_options(int);

int
test_delta_native_0210(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_or_options(3);
}
