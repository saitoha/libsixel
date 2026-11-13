/* SPDX-License-Identifier: MIT */
/* Verify adaptive a dither gpu bytes. */
int test_refresh_adaptive_gpu(char const *);

int
test_delta_refresh_0081(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_gpu("a_dither");
}
