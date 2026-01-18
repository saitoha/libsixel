/*
 * Test harness for cli_guard_missing_argument handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "converters/cli.h"

typedef struct guard_result {
    int code;
    int missing_calls;
    int rewound_optind;
} guard_result_t;

static int g_missing_calls;
static int g_rewound;

static int
allows_leading_dash(int short_opt, void *user_data)
{
    (void)user_data;
    return short_opt == 'i';
}

static void
report_missing(int short_opt, void *user_data)
{
    int *last_short;

    last_short = (int *)user_data;
    g_missing_calls += 1;
    *last_short = short_opt;
}

static guard_result_t
run_guard_case(char *const *argv,
               char *argument,
               int *optind_ptr,
               char const *optstring,
               cli_option_help_t const *table,
               size_t table_count,
               int allow_dash)
{
    int report_short;
    int result;
    int starting_optind;

    report_short = 0;
    g_missing_calls = 0;
    g_rewound = 0;

    starting_optind = 0;
    if (optind_ptr != NULL) {
        starting_optind = *optind_ptr;
    }

    result = cli_guard_missing_argument(
        'i',
        argv,
        argument,
        optind_ptr,
        optstring,
        table,
        table_count,
        allow_dash ? allows_leading_dash : NULL,
        NULL,
        report_missing,
        &report_short);

    if (optind_ptr != NULL
            && *optind_ptr == starting_optind - 1
            && allow_dash == 0) {
        g_rewound = 1;
    }

    return (guard_result_t){ result, g_missing_calls, g_rewound };
}

int
test_cli_0031_cli_guard_missing_argument(int argc, char **argv)
{
    cli_option_help_t const table[] = {
        { 'i', "input", "--input help\n" },
        { 'x', "extract", "--extract help\n" },
    };
    char argv0[] = "tool";
    char argv1[] = "-x";
    char dash_value[] = "-file.six";
    char *args[] = { argv0, argv1, NULL };
    size_t table_count;
    int optind_value;
    guard_result_t result;
    int status;

    (void) argc;
    (void) argv;

    table_count = sizeof(table) / sizeof(table[0]);

    optind_value = 1;
    result = run_guard_case(args,
                             NULL,
                             &optind_value,
                             "i:",
                             table,
                             table_count,
                             0);
    status = 0;
    if (result.code == -1 && result.missing_calls == 1) {
    } else {
        fprintf(stderr, "case 1: missing argument not reported\n");
        status = 1;
    }

    optind_value = 0;
    result = run_guard_case(args,
                             dash_value,
                             &optind_value,
                             "i:",
                             table,
                             table_count,
                             1);
    if (result.code == 0 && result.missing_calls == 0) {
    } else {
        fprintf(stderr, "case 2: leading dash rejected\n");
        status = 1;
    }

    optind_value = 2;
    result = run_guard_case(args,
                             args[1],
                             &optind_value,
                             "i:",
                             table,
                             table_count,
                             0);
    if (result.code == -1 && result.missing_calls == 1 &&
            result.rewound_optind != 0) {
    } else {
        fprintf(stderr, "case 3: did not rewind recognised option\n");
        status = 1;
    }

    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
