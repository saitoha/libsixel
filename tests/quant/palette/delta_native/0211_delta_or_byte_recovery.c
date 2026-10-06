/* SPDX-License-Identifier: MIT */
/* Verify or byte recovery. */
int test_delta_or_recovery(int);

int
test_delta_native_0211(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_or_recovery(0);
}
