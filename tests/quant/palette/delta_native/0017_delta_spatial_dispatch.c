/* SPDX-License-Identifier: MIT */
/* All admitted spatial methods must reach native delta at high level. */
int test_delta_native_frames(int);

int
test_delta_native_0017(int argc, char **argv)
{
    int mode;

    (void)argc;
    (void)argv;
    for (mode = 10; mode < 20; ++mode) {
        if (test_delta_native_frames(mode) != 0) {
            return 1;
        }
    }
    return 0;
}
