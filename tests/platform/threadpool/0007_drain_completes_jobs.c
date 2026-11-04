/* SPDX-License-Identifier: MIT */
/* drain completes jobs. */
int pool_reuse_observe(int);

int
test_0007_drain_completes_jobs(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return pool_reuse_observe(0);
}
