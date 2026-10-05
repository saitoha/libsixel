/* SPDX-License-Identifier: MIT */
/* Native blue delta: frames contract. */
int test_delta_positional_frames(int, int);

int
test_delta_native_0023(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_positional_frames(2, 0) ||
        test_delta_positional_frames(2, 2) ||
        test_delta_positional_frames(2, 3);
}
