/* SPDX-License-Identifier: MIT */
/* Source masks retain coordinates across transparent margins. */
int test_highcolor_mask(int);

int
test_highcolor_0059(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(59);
}
