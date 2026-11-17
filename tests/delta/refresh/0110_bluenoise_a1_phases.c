/* SPDX-License-Identifier: MIT */
int test_refresh_bluenoise_unchanged(unsigned int);

int
test_delta_refresh_0110(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_bluenoise_unchanged(1);
}
