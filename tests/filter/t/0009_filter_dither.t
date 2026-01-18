#!/bin/sh
# Run the prebuilt filter unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eu

if [ "${CROSS_COMPILING:-no}" = "yes" ]; then
    echo "1..0 # SKIP cross compiling"
    exit 0
fi

if [ -z "${TOP_BUILDDIR:-}" ]; then
    echo "Bail out! missing TOP_BUILDDIR" 1>&2
    exit 1
fi

BIN="${TOP_BUILDDIR}/tests/test_runner"

if [ ! -x "${BIN}" ]; then
    echo "Bail out! missing test binary: ${BIN}" 1>&2
    exit 1
fi

test_name=$(basename "$0" .t)
log_file=$(mktemp "${TMPDIR:-/tmp}/${test_name}.XXXXXX")
trap 'rm -f "${log_file}"' EXIT

set +e
"${BIN}" "filter/${test_name}" >"${log_file}" 2>&1
rc=$?
set -e

echo "1..1"

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - ${test_name}"
elif [ "${rc}" -eq 77 ]; then
    echo "ok 1 - ${test_name} # SKIP unavailable"
else
    echo "not ok 1 - ${test_name}"
    sed 's/^/# /' "${log_file}"
    exit 1
fi
