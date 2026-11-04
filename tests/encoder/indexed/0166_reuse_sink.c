/* SPDX-License-Identifier: MIT */
/* reuse sink. */
int core_reuse_observe(int);

int
test_0166_reuse_sink(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(3);
}
