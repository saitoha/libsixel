/* SPDX-License-Identifier: MIT */
/* Preserve the gap ten body recorded at e21311523. */
int test_compose_stream(int, char const *);

int
test_compose_0046(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_compose_stream(1, "#0@#1!4?@#0!5?@");
}
