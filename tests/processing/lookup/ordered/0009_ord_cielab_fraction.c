/* SPDX-License-Identifier: MIT */
/* One native-float observation using the independent shared oracle. */
int test_extended_lookup(int argc, char **argv);

int
test_ordered_lookup_0009(int argc, char **argv)
{
    char *args[] = { "lookup", "fraction", "7", "3" };

    (void)argc;
    (void)argv;
    return test_extended_lookup(4, args);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
