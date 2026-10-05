/* SPDX-License-Identifier: MIT */
/* Native blue delta: byte contract. */
int test_delta_positional_frames(int, int);

int
test_delta_native_0029(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_positional_frames(2, 1);
}
