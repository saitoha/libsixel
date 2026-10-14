/* SPDX-License-Identifier: MIT */
/* Preserve the band reset body recorded at e21311523. */
int test_compose_stream(int, char const *);

int
test_compose_0048(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_compose_stream(3, "#0@-#1!64?@-#0!129?@");
}
