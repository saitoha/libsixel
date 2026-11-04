/* SPDX-License-Identifier: MIT */
/* open rejects resume. */
int pool_reuse_observe(int);

int
test_0013_open_rejects_resume(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return pool_reuse_observe(6);
}
