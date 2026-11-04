/* SPDX-License-Identifier: MIT */
/* resume rebinds policy. */
int pool_reuse_observe(int);

int
test_0010_resume_rebinds_policy(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return pool_reuse_observe(3);
}
