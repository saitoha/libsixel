/* SPDX-License-Identifier: MIT */
/* Verify drcs p3 recovery. */
int test_delta_drcs_recovery(int);

int
test_delta_native_0221(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_drcs_recovery(2);
}
