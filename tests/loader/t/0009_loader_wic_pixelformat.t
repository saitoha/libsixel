#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
parent_dir=$(CDPATH=; cd "${script_dir}/../.." && pwd)

if [ -n "${MESON_BUILD_ROOT:-}" ]; then
    top_builddir=${TOP_BUILDDIR:-${MESON_BUILD_ROOT}}
else
    top_builddir=${TOP_BUILDDIR:-${parent_dir}/..}
fi

. "${script_dir}/../../common/t/0001_converters_common.t"

runner="${top_builddir}/tests/test_runner${SIXEL_BIN_EXT-}"
if [ ! -x "${runner}" ] && [ -z "${SIXEL_RUNTIME-}" ]; then
    echo "Bail out! missing test binary: ${runner}" 1>&2
    exit 1
fi

test_name=$(basename "$0" .t)
log_file=$(mktemp "${TMPDIR:-/tmp}/loader-${test_name}.XXXXXX")
trap 'rm -f "${log_file}"' EXIT

set +e
${SIXEL_RUNTIME-} "${runner}" "loader/${test_name}" >"${log_file}" 2>&1
rc=$?
set -e

if grep "{cacaf262-9370-4615-a13b-9f5539da4c0a} not registered" "${log_file}" \
    > /dev/null; then
    skip_all "WIC is not available"
fi

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - loader/${test_name}"
elif [ "${rc}" -eq 77 ]; then
    echo "ok 1 - loader/${test_name} # SKIP unavailable"
else
    echo "not ok 1 - loader/${test_name}"
    sed 's/^/# /' "${log_file}"
    exit 1
fi
