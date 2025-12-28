#!/bin/sh
# TAP test covering cli_token_is_known_option with short/long tokens.

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

cat >"${build_dir}/cli_token_cases.c" <<'C_EOF'
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
            print_result((int)(index + 1u), 0,
                         "recognition mismatch");
            status = 1;
            continue;
        }
        if (cases[index].expected_recognised != 0) {
            if (out_short == cases[index].expected_short_opt) {
                print_result((int)(index + 1u), 1,
                             "recognised option");
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
C_EOF

binary="${build_dir}/cli_token_cases"

${cc} ${cflags} "${top_srcdir}/converters/cli.c" \
    "${build_dir}/cli_token_cases.c" -o "${binary}" ${ldflags} \
    >"${log_file}" 2>&1

export LD_LIBRARY_PATH="${ldpath}:${LD_LIBRARY_PATH:-}"

"${binary}" | tee "${artifact_dir}/tap.log"
