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

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

runner="${top_builddir}/tests/test_runner${SIXEL_BIN_EXT-}"
if [ ! -x "${runner}" ] && [ -z "${SIXEL_RUNTIME-}" ]; then
    echo "Bail out! missing test binary: ${runner}" 1>&2
    exit 1
fi

set +e
loader_output=$(${SIXEL_RUNTIME-} "${runner}" "loader/${test_name}" 2>&1)
rc=$?
set -e
printf '%s' "${loader_output}" >&2

if printf '%s' "${loader_output}" \
    | grep "{cacaf262-9370-4615-a13b-9f5539da4c0a} not registered" \
        >/dev/null; then
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
    exit 1
fi
