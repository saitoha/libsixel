#!/bin/sh
# TAP test for cli_guard_missing_argument handling of missing/leading-dash.

set -eux

name=${0##*[/\\]}

binary="${TOP_BUILDDIR}/tests/test_runner${SIXEL_BIN_EXT-}"
if [ ! -x "${binary}" ]; then
    echo "harness not built" >&2
    exit 99
fi

set +e
cli_output=$(${SIXEL_RUNTIME-} "${binary}" "cli/0031_cli_guard_missing_argument" \
    2>&1)
rc=$?
set -e
printf '%s' "${cli_output}" >&2

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - cli_guard_missing_argument"
else
    echo "not ok 1 - cli_guard_missing_argument"
    exit 1
fi
