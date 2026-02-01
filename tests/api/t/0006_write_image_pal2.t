#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
parent_dir=$(CDPATH=; cd "${script_dir}/../.." && pwd)

if [ -n "${MESON_BUILD_ROOT:-}" ]; then
    top_builddir=${TOP_BUILDDIR:-${MESON_BUILD_ROOT}}
else
    top_builddir=${TOP_BUILDDIR:-${parent_dir}/..}
fi

runner="${top_builddir}/tests/test_runner${SIXEL_BIN_EXT-}"
test_name=$(basename "$0" .t)

set +e
${SIXEL_RUNTIME-} "${runner}" "api/${test_name}"
rc=$?
set -e

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - api/${test_name}"
elif [ "${rc}" -eq 77 ]; then
    echo "ok 1 - api/${test_name} # SKIP unavailable"
else
    echo "not ok 1 - api/${test_name}"
    sed 's/^/# /' "${log_file}"
    exit 1
fi
