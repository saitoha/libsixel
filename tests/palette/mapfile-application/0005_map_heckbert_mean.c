/* SPDX-License-Identifier: MIT */
int test_map_cluster(int mode, char const *path);

int
test_map_cluster_0005(int argc, char **argv)
{
    return argc == 2 ? test_map_cluster(4, argv[1]) : 1;
}
