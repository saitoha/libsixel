/* SPDX-License-Identifier: MIT */
/* Verify delta byte control. */
int test_delta_native_frames(int);

int
test_delta_native_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_native_frames(1);
}
