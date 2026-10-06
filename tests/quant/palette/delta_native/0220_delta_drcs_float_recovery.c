/* SPDX-License-Identifier: MIT */
/* Verify drcs float recovery. */
int test_delta_drcs_recovery(int);

int
test_delta_native_0220(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_drcs_recovery(1);
}
