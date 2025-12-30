/*
 * Test harness for cli_token_is_known_option.
 */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "converters/cli.h"

typedef struct test_case {
    char const *token;
    int expected_recognised;
    int expected_short_opt;
} test_case_t;

static cli_option_help_t const g_help[] = {
    { 'a', "alpha", "alpha option\n" },
    { 'b', "bravo", "bravo option\n" },
};

static void
print_result(int index, int success, char const *message)
{
    printf("%s %d - %s\n",
           success ? "ok" : "not ok",
           index,
           message);
}

int
main(void)
{
    test_case_t cases[] = {
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

    memset(long_token, 'x', sizeof(long_token));
    long_token[0] = '-';
    long_token[1] = '-';
    long_token[70] = '\0';
    cases[9].token = long_token;

    printf("1..%zu\n", sizeof(cases) / sizeof(cases[0]));

    status = 0;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        out_short = -99;
        recognised = cli_token_is_known_option(
            g_help,
            sizeof(g_help) / sizeof(g_help[0]),
            cases[index].token,
            &out_short);
        if (recognised != cases[index].expected_recognised) {
            print_result((int)(index + 1u), 0, "recognition mismatch");
            status = 1;
            continue;
        }
        if (cases[index].expected_recognised != 0) {
            if (out_short == cases[index].expected_short_opt) {
                print_result((int)(index + 1u), 1, "recognised option");
            } else {
                print_result((int)(index + 1u), 0,
                             "short option mismatch");
                status = 1;
            }
        } else {
            if (out_short == 0) {
                print_result((int)(index + 1u), 1,
                             "unknown resets output");
            } else {
                print_result((int)(index + 1u), 0,
                             "unexpected short option set");
                status = 1;
            }
        }
    }

    return status;
}
