/* SPDX-License-Identifier: MIT */
/* Working-space conversion admits required float with AUTO. */
int test_auto_cover_admission(int);

int
test_auto_cover_0015(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(5);
}
