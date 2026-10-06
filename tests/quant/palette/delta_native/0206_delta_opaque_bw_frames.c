/* SPDX-License-Identifier: MIT */
/* Verify opaque bw frames. */
int test_delta_bw_frames(int);

int
test_delta_native_0206(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_bw_frames(0);
}
