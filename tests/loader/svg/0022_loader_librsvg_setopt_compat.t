#!/bin/sh
# TAP wrapper that dispatches setopt-compat coverage to the C test runner.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0026_loader_librsvg_setopt_compat" || rc="$?"

test "${rc-}" = 77 && {
    printf '\n%s\n' "ok 1 - loader/0026_loader_librsvg_setopt_compat # SKIP unavailable"
    exit 0
}

test -n "${rc-}" && {
    printf '\n%s\n' "not ok 1 - loader/0026_loader_librsvg_setopt_compat"
    exit 0
}

printf '\n%s\n' "ok 1 - loader/0026_loader_librsvg_setopt_compat"
exit 0
