#!/bin/sh
# TAP wrapper confirming librsvg ignored setopt diagnostics in debug builds.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

echo "1..1"
set -v

rc=0
msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
        "loader/0032_loader_librsvg_setopt_ignored_diag" 2>&1
) || rc="$?"

test "${rc-}" = 77 && {
    echo "ok 1 - loader/0032_loader_librsvg_setopt_ignored_diag # SKIP unavailable"
    exit 0
}

case "${msg}" in
    *"unknown test:"*)
        echo "ok 1 - loader/0032_loader_librsvg_setopt_ignored_diag # SKIP runner without librsvg C tests"
        exit 0
        ;;
esac

test "${rc-}" -ne 0 && {
    echo "not ok 1 - loader/0032_loader_librsvg_setopt_ignored_diag"
    exit 0
}

echo "ok 1 - loader/0032_loader_librsvg_setopt_ignored_diag"
exit 0
