/*
 * Test harness for cli_token_is_known_option.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "converters/cli.h"

typedef struct cli_token_case {
    char const *token;
    int expected_recognised;
    int expected_short_opt;
} cli_token_case_t;

static cli_option_help_t const g_help[] = {
    { 'a', "alpha", "alpha option\n" },
    { 'b', "bravo", "bravo option\n" },
};

int
test_cli_0029_cli_token_is_known_option(int argc, char **argv)
{
    cli_token_case_t cases[] = {
        { "-a", 1, 'a' },
        { "--alpha", 1, 'a' },
        { "--alpha=1", 1, 'a' },
        { "-", 0, 0 },
        { "--", 0, 0 },
        { "---bad", 0, 0 },
        { "plain", 0, 0 },
        { "-ab", 1, 'a' },
        { "--bravo", 1, 'b' },
        { NULL, 0, 0 },
    };
    char long_token[80];
    size_t index;
    int status;
    int out_short;
    int recognised;

    (void) argc;
    (void) argv;

    memset(long_token, 'x', sizeof(long_token));
    long_token[0] = '-';
    long_token[1] = '-';
    long_token[70] = '\0';
    cases[9].token = long_token;

    status = 0;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        out_short = -99;
        recognised = cli_token_is_known_option(
            g_help,
            sizeof(g_help) / sizeof(g_help[0]),
            cases[index].token,
            &out_short);
        if (recognised != cases[index].expected_recognised) {
            fprintf(stderr, "case %zu: recognition mismatch\n", index + 1u);
            status = 1;
            continue;
        }
        if (cases[index].expected_recognised != 0 &&
                out_short != cases[index].expected_short_opt) {
            fprintf(stderr, "case %zu: short option mismatch\n",
                    index + 1u);
            status = 1;
        } else if (cases[index].expected_recognised == 0 && out_short != 0) {
            fprintf(stderr, "case %zu: unexpected short option set\n",
                    index + 1u);
            status = 1;
        }
    }

    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
