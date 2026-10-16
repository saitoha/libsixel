/* SPDX-License-Identifier: MIT */
int fhedt_reuse_isolation(int floating);

int
test_fh_lookup_0028(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return fhedt_reuse_isolation(1);
}
