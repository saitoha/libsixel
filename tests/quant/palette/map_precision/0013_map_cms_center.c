/* SPDX-License-Identifier: MIT */
/* Retain independent map precision with K-center construction. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0013(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(0, 0,
        "8bit", "center:seed=1",
        "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!",
        argv[1]) : 1;
}
