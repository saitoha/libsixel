#!/bin/sh
# TAP test for cli_guard_missing_argument handling of missing/leading-dash.

set -euxv

name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${name}"
build_dir="${artifact_dir}/build"
log_file="${artifact_dir}/compile.log"

mkdir -p "${build_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
parent_dir=$(CDPATH=; cd "${script_dir}/.." && pwd)

if [ -n "${MESON_SOURCE_ROOT:-}" ]; then
    top_srcdir=${TOP_SRCDIR:-${MESON_SOURCE_ROOT}}
else
    top_srcdir=${TOP_SRCDIR:-${parent_dir}}
fi

if [ -n "${MESON_BUILD_ROOT:-}" ]; then
    top_builddir=${TOP_BUILDDIR:-${MESON_BUILD_ROOT}}
else
    top_builddir=${TOP_BUILDDIR:-${parent_dir}}
fi

pc_file="${top_builddir}/libsixel.pc"
if [ -f "${pc_file}" ]; then
    PKG_CONFIG_PATH="${top_builddir}${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"
    export PKG_CONFIG_PATH
fi

cc=${CC:-cc}

cflags="-std=c99 -Wall -Wextra -Werror -DHAVE_CONFIG_H"
cflags="${cflags} -I${top_builddir} -I${top_srcdir} -I${top_srcdir}/converters"

ldflags="-L${top_builddir}/src/.libs -lsixel"
ldpath="${top_builddir}/src/.libs:${top_builddir}/src"

if pkg-config --exists libsixel; then
    cflags="$(pkg-config --cflags libsixel) ${cflags}"
    ldflags="$(pkg-config --libs libsixel) ${ldflags}"
fi

cat >"${build_dir}/cli_guard_missing.c" <<'C_EOF'
#include <stdio.h>
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
    cli_option_help_t const table[] = {
        { 'i', "input", "--input help\n" },
        { 'x', "extract", "--extract help\n" },
    };
    char dash_value[] = "-file.six";
    char const *argv[] = { "tool", "-x", NULL };
    size_t table_count;
    int optind_value;
    guard_result_t result;
    int status;

    table_count = sizeof(table) / sizeof(table[0]);

    printf("1..3\n");

    optind_value = 1;
    result = run_guard_case(argv,
                             NULL,
                             &optind_value,
                             "i:",
                             table,
                             table_count,
                             0);
    status = 0;
    if (result.code == -1 && result.missing_calls == 1) {
        print_result(1, 1, "reports missing argument");
    } else {
        print_result(1, 0, "missing argument not reported");
        status = 1;
    }

    optind_value = 0;
    result = run_guard_case(argv,
                             dash_value,
                             &optind_value,
                             "i:",
                             table,
                             table_count,
                             1);
    if (result.code == 0 && result.missing_calls == 0) {
        print_result(2, 1, "leading dash accepted as value");
    } else {
        print_result(2, 0, "leading dash rejected");
        status = 1;
    }

    optind_value = 2;
    result = run_guard_case(argv,
                             (char *)argv[1],
                             &optind_value,
                             "i:",
                             table,
                             table_count,
                             0);
    if (result.code == -1 && result.missing_calls == 1 &&
            result.rewound_optind != 0) {
        print_result(3, 1, "rewinds recognised option");
    } else {
        print_result(3, 0, "did not rewind recognised option");
        status = 1;
    }

    return status;
}
C_EOF

binary="${build_dir}/cli_guard_missing"

${cc} ${cflags} "${top_srcdir}/converters/cli.c" \
    "${build_dir}/cli_guard_missing.c" -o "${binary}" ${ldflags} \
    >"${log_file}" 2>&1

export LD_LIBRARY_PATH="${ldpath}:${LD_LIBRARY_PATH:-}"

"${binary}" | tee "${artifact_dir}/tap.log"
