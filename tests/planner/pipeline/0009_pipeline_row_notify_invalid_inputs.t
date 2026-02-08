#!/bin/sh
# TAP wrapper for direct C API coverage of pipeline row notify edge inputs.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

if [ "${CROSS_COMPILING:-no}" = "yes" ]; then
    echo "1..0 # SKIP cross compiling"
    exit 0
fi

if [ -z "${TOP_BUILDDIR:-}" ]; then
    echo "Bail out! missing TOP_BUILDDIR" 1>&2
    exit 1
fi

bin="${TEST_RUNNER_PATH}"
if [ ! -x "${bin}" ] && [ -z "${SIXEL_RUNTIME-}" ]; then
    echo "Bail out! missing test binary: ${bin}" 1>&2
    exit 1
fi

set +e
api_output=$(run_test_runner "pipeline/${test_name}" 2>&1)
rc=$?
set -e
printf '%s' "${api_output}" >&2

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - ${test_name}"
elif [ "${rc}" -eq 77 ]; then
    echo "ok 1 - ${test_name} # SKIP unavailable"
else
    echo "not ok 1 - ${test_name}"
    exit 1
fi
