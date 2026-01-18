/*
 * Test harness for cli_option_requires_argument optstring parsing.
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "converters/cli.h"

typedef struct cli_requires_case {
    int short_opt;
    int expected;
} cli_requires_case_t;

int
test_cli_0030_cli_option_requires_argument(int argc, char **argv)
{
    char const optstring[] = "a:b::c";
    cli_requires_case_t cases[] = {
        { 'a', 1 },
        { 'b', 1 },
        { 'c', 0 },
        { 'z', 0 },
    };
    size_t index;
    int status;
    int requires;

    (void) argc;
    (void) argv;

    status = 0;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        requires = cli_option_requires_argument(optstring,
                                                cases[index].short_opt);
        if (requires != cases[index].expected) {
            fprintf(stderr, "case %zu: optstring mismatch\n", index + 1u);
            status = 1;
        }
    }

    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
