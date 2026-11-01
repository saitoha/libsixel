/* SPDX-License-Identifier: MIT */
/* Environment AUTO admits the native palette path. */
int test_auto_cover_admission(int);

int
test_auto_cover_0016(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(6);
}
