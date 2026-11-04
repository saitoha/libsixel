/* SPDX-License-Identifier: MIT */
/* resume rebinds hooks. */
int pool_reuse_observe(int);

int
test_0011_resume_rebinds_hooks(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return pool_reuse_observe(4);
}
