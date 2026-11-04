/* SPDX-License-Identifier: MIT */
/* resume keeps thread. */
int pool_reuse_observe(int);

int
test_0009_resume_keeps_thread(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return pool_reuse_observe(2);
}
