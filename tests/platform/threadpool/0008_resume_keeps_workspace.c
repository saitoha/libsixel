/* SPDX-License-Identifier: MIT */
/* resume keeps workspace. */
int pool_reuse_observe(int);

int
test_0008_resume_keeps_workspace(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return pool_reuse_observe(1);
}
