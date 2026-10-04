/* SPDX-License-Identifier: MIT */
/* Animation re-evaluates byte and float admission on each frame. */
int test_auto_cover_admission(int);

int
test_auto_cover_0023(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(13);
}
