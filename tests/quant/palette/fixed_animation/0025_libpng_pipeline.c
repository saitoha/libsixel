/* SPDX-License-Identifier: MIT */
/* Verify four fixed-palette APNG frames and actual queued replay. */
int test_fixed_animation_loader(char const *, char const *);

int
test_fixed_animation_0025(int argc, char **argv)
{
    return argc == 3 ? test_fixed_animation_loader(argv[1], argv[2]) : 1;
}
