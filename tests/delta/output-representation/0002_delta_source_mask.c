/* SPDX-License-Identifier: MIT */
/* Verify delta source mask. */
int test_delta_native_frames(int);

int
test_delta_native_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_native_frames(3);
}
