/* SPDX-License-Identifier: MIT */
/* Verify adaptive none gpu bytes. */
int test_refresh_adaptive_gpu(char const *);

int
test_delta_refresh_0080(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_gpu("none");
}
