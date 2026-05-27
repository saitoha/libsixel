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
    int rewind_by;
} guard_result_t;

static int g_missing_calls;
static int g_rewind_by;

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
    g_rewind_by = 0;

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
            && *optind_ptr < starting_optind
            && allow_dash == 0) {
        g_rewind_by = starting_optind - *optind_ptr;
    }

    return (guard_result_t){ result, g_missing_calls, g_rewind_by };
}

int
test_cli_0031_cli_guard_missing_argument(int argc, char **argv)
{
    cli_option_help_t const table[] = {
        { 'i', "input", "--input help\n" },
        { 'x', "extract", "--extract help\n" },
        { '1', "show-completion", "--show-completion help\n" },
    };
    char argv0[] = "tool";
    char argv1[] = "-x";
    char argv2[] = "dummy";
    char argv3[] = "far";
    char argv4[] = "away";
    char dash_value[] = "-file.six";
    char copied_option[] = "-x";
    char negative_value[] = "-1";
    char attached_negative[] = "--input=-1";
    char *args[] = { argv0, argv1, NULL };
    char *lagged_args[] = { argv0, argv1, argv2, NULL };
    char *far_args[] = { argv0, argv1, copied_option, argv3, argv4, NULL };
    char *attached_args[] = { argv0, attached_negative, NULL };
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
            result.rewind_by == 1) {
    } else {
        fprintf(stderr, "case 3: did not rewind recognised option\n");
        status = 1;
    }

    optind_value = 2;
    result = run_guard_case(args,
                             copied_option,
                             &optind_value,
                             "i:",
                             table,
                             table_count,
                             0);
    if (result.code == -1 && result.missing_calls == 1 &&
            result.rewind_by == 1) {
    } else {
        fprintf(stderr, "case 4: copied argument did not rewind\n");
        status = 1;
    }

    optind_value = 3;
    result = run_guard_case(lagged_args,
                             copied_option,
                             &optind_value,
                             "i:",
                             table,
                             table_count,
                             0);
    if (result.code == -1 && result.missing_calls == 1 &&
            result.rewind_by == 2) {
    } else {
        fprintf(stderr, "case 5: lagged optind did not rewind by two\n");
        status = 1;
    }

    optind_value = 5;
    result = run_guard_case(far_args,
                             copied_option,
                             &optind_value,
                             "i:",
                             table,
                             table_count,
                             0);
    if (result.code == -1 && result.missing_calls == 1 &&
            result.rewind_by == 0) {
    } else {
        fprintf(stderr, "case 6: far optind accepted recognised option\n");
        status = 1;
    }

    optind_value = 2;
    result = run_guard_case(attached_args,
                             negative_value,
                             &optind_value,
                             "i:1:",
                             table,
                             table_count,
                             0);
    if (result.code == 0 && result.missing_calls == 0) {
    } else {
        fprintf(stderr, "case 7: attached negative value was rejected\n");
        status = 1;
    }

    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
