/* SPDX-License-Identifier: MIT */
/* failed rejects resume. */
int pool_reuse_observe(int);

int
test_0015_failed_rejects_resume(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return pool_reuse_observe(8);
}
