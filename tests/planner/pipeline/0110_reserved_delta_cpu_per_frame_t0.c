/* SPDX-License-Identifier: MIT */
int reserved_delta_test(int gpu, int latest, int threshold);

int
test_reserved_delta_0110(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return reserved_delta_test(0, 0, 0);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
