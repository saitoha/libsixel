/* SPDX-License-Identifier: MIT */
/* reuse geometry rebuild. */
int core_reuse_observe(int);

int
test_0179_reuse_geometry_rebuild(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(16);
}
