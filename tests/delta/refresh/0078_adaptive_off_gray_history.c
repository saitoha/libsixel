/* SPDX-License-Identifier: MIT */
/* Verify adaptive off gray history. */
int test_refresh_adaptive_pixels(char const *);

int
test_delta_refresh_0078(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_pixels("off");
}
