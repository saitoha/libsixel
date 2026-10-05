/* SPDX-License-Identifier: MIT */
/* Verify delta plane origin. */
int test_delta_native_frames(int);

int
test_delta_native_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_native_frames(2);
}
