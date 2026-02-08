#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
parent_dir=$(CDPATH=; cd "${script_dir}/../.." && pwd)

if [ -n "${MESON_BUILD_ROOT:-}" ]; then
    top_builddir=${TOP_BUILDDIR:-${MESON_BUILD_ROOT}}
else
    top_builddir=${TOP_BUILDDIR:-${parent_dir}/..}
fi

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

runner="${TEST_RUNNER_PATH}"
if [ ! -x "${runner}" ] && [ -z "${SIXEL_RUNTIME-}" ]; then
    echo "Bail out! missing test binary: ${runner}" 1>&2
    exit 1
fi

set +e
loader_output=$(run_test_runner "loader/${test_name}" 2>&1)
rc=$?
set -e
printf '%s' "${loader_output}" >&2

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - loader/${test_name}"
elif [ "${rc}" -eq 77 ]; then
    echo "ok 1 - loader/${test_name} # SKIP unavailable"
else
    echo "not ok 1 - loader/${test_name}"
    exit 1
fi
