/* SPDX-License-Identifier: MIT */
int test_filtered_6bit(int argc, char **argv);

int
test_filtered_0008(int argc, char **argv)
{
    char *args[] = { "filtered", "3" };

    (void)argc;
    (void)argv;
    return test_filtered_6bit(2, args);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
