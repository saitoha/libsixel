/* SPDX-License-Identifier: MIT */
/* Native x delta: frames contract. */
int test_delta_positional_frames(int, int);

int
test_delta_native_0022(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_positional_frames(1, 0) ||
        test_delta_positional_frames(1, 2) ||
        test_delta_positional_frames(1, 3);
}
