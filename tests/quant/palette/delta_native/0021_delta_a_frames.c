/* SPDX-License-Identifier: MIT */
/* Native a delta: frames contract. */
int test_delta_positional_frames(int, int);

int
test_delta_native_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_positional_frames(0, 0) ||
        test_delta_positional_frames(0, 2) ||
        test_delta_positional_frames(0, 3);
}
