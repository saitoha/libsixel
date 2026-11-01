/* SPDX-License-Identifier: MIT */
/* Verify macro p3 recovery. */
int test_delta_macro_recovery(int);

int
test_delta_native_0232(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_macro_recovery(2);
}
