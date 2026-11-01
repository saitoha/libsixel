/* SPDX-License-Identifier: MIT */
int test_progressive_callback_mode(char const *path, int worker);

int
test_progressive_0016(int argc, char **argv)
{
    return argc == 2 ? test_progressive_callback_mode(argv[1], 1) : 1;
}
