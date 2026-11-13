/* SPDX-License-Identifier: MIT */
/* Verify adaptive x dither gpu bytes. */
int test_refresh_adaptive_gpu(char const *);

int
test_delta_refresh_0082(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_gpu("x_dither");
}
