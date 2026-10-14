/* SPDX-License-Identifier: MIT */
/* Preserve the word boundaries body recorded at e21311523. */
int test_compose_stream(int, char const *);

int
test_compose_0047(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_compose_stream(2, "#0@!62?@@#1!62?@#0!127?@@");
}
