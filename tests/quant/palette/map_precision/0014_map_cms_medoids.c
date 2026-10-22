/* SPDX-License-Identifier: MIT */
/* Retain independent map precision with K-medoids construction. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0014(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(0, 0,
        "auto", "medoids:seed=1",
        "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!",
        argv[1]) : 1;
}
