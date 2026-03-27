#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic loader is unavailable\n"
    exit 0
}
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n"
    exit 0
}


echo "1..1"
set -v

loader_output=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/0009_loader_wic_pixelformat" 2>&1) || rc=$?
printf '%s' "${loader_output}" >&2

test "${rc-0}" -eq 0 || {
    echo "not ok" 1 - "loader/0009_loader_wic_pixelformat"
    exit 0
}

echo "ok" 1 - "loader/0009_loader_wic_pixelformat"
exit 0
