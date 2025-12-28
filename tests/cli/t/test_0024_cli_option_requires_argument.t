#!/bin/sh
# TAP test for cli_option_requires_argument optstring parsing.

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

cat >"${build_dir}/cli_requires_arg.c" <<'C_EOF'
#include <stdio.h>

#include "config.h"
#include "converters/cli.h"

typedef struct test_case {
    int short_opt;
    int expected;
} test_case_t;

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
    char const optstring[] = "a:b::c";
    test_case_t cases[] = {
        { 'a', 1 },
        { 'b', 1 },
        { 'c', 0 },
        { 'z', 0 },
    };
    size_t index;
    int status;
    int requires;

    printf("1..%zu\n", sizeof(cases) / sizeof(cases[0]));

    status = 0;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        requires = cli_option_requires_argument(optstring,
                                                cases[index].short_opt);
        if (requires == cases[index].expected) {
            print_result((int)(index + 1u), 1, "optstring parsed");
        } else {
            print_result((int)(index + 1u), 0, "optstring mismatch");
            status = 1;
        }
    }

    return status;
}
C_EOF

binary="${build_dir}/cli_requires_arg"

${cc} ${cflags} "${top_srcdir}/converters/cli.c" \
    "${build_dir}/cli_requires_arg.c" -o "${binary}" ${ldflags} \
    >"${log_file}" 2>&1

export LD_LIBRARY_PATH="${ldpath}:${LD_LIBRARY_PATH:-}"

"${binary}" | tee "${artifact_dir}/tap.log"
