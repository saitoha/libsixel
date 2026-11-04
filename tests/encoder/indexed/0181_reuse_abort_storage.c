/* SPDX-License-Identifier: MIT */
/* reuse abort storage. */
int core_reuse_observe(int);

int
test_0181_reuse_abort_storage(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return core_reuse_observe(18);
}
