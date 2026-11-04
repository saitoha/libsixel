/* SPDX-License-Identifier: MIT */
/* finish drains jobs. */
int pool_reuse_observe(int);

int
test_0020_finish_drains_jobs(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return pool_reuse_observe(13);
}
