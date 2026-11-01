/* SPDX-License-Identifier: MIT */
/* Native a delta: byte contract. */
int test_delta_positional_frames(int, int);

int
test_delta_native_0027(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_positional_frames(0, 1);
}
