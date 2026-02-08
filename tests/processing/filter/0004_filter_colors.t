#!/bin/sh
# Run the prebuilt filter unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

if [ "${CROSS_COMPILING:-no}" = "yes" ]; then
    echo "1..0 # SKIP cross compiling"
    exit 0
fi

if [ -z "${TOP_BUILDDIR:-}" ]; then
    echo "Bail out! missing TOP_BUILDDIR" 1>&2
    exit 1
fi

BIN="${TEST_RUNNER_PATH}"

if [ ! -x "${BIN}" ] && [ -z "${SIXEL_RUNTIME-}" ]; then
    echo "Bail out! missing test binary: ${BIN}" 1>&2
    exit 1
fi


set +e
filter_output=$(run_test_runner "filter/${test_name}" 2>&1)
rc=$?
set -e
printf '%s' "${filter_output}" >&2

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
