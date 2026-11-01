/* SPDX-License-Identifier: MIT */
/* Verify drcs byte recovery. */
int test_delta_drcs_recovery(int);

int
test_delta_native_0219(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_drcs_recovery(0);
}
