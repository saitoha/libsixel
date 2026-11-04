/* SPDX-License-Identifier: MIT */
/* reuse release storage. */
int core_reuse_observe(int);

int
test_0180_reuse_release_storage(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(17);
}
