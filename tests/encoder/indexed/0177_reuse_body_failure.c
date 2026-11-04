/* SPDX-License-Identifier: MIT */
/* reuse body failure. */
int core_reuse_observe(int);

int
test_0177_reuse_body_failure(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(14);
}
