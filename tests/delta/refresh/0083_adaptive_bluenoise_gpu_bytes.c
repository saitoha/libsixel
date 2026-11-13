/* SPDX-License-Identifier: MIT */
/* Verify adaptive bluenoise gpu bytes. */
int test_refresh_adaptive_gpu(char const *);

int
test_delta_refresh_0083(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_gpu("bluenoise");
}
