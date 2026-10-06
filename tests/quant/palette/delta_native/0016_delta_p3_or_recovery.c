/* SPDX-License-Identifier: MIT */
/* Verify delta p3 or rejection recovery. */
int test_delta_native_scope(int);

int
test_delta_native_0016(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_native_scope(0);
}
