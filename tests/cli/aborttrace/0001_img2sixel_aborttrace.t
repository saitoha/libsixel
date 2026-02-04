#!/bin/sh
# TAP test verifying abort trace output from the runner test.

set -eux

if [ "x${SIXEL_TSAN_BUILD:-no}" = "xyes" ]; then
    echo "1..0 # SKIP TSan builds can suppress abort trace output"
    exit 0
fi

name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_test_dir=$(dirname "$0")
artifact_dir="${artifact_root}/${artifact_test_dir}/${name}"
mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
parent_dir=$(CDPATH=; cd "${script_dir}/../.." && pwd)

if [ -n "${MESON_BUILD_ROOT:-}" ]; then
    top_builddir=${TOP_BUILDDIR:-${MESON_BUILD_ROOT}}
else
    top_builddir=${TOP_BUILDDIR:-${parent_dir}}
fi

binary="${top_builddir}/tests/test_runner${SIXEL_BIN_EXT-}"
if [ ! -x "${binary}" ] && [ -z "${SIXEL_RUNTIME-}" ]; then
    echo "harness not built" >&2
    exit 99
fi

log_file="${artifact_dir}/test.log"
set +e
SIXEL_ABORT_TRACE=1 ${SIXEL_RUNTIME-} "${binary}" \
    "aborttrace/0001_img2sixel_aborttrace" >"${log_file}" 2>&1
rc=$?
set -e

echo "1..1"
set -v

if [ "${rc}" -eq 77 ]; then
    echo "ok 1 - abort trace # SKIP abort trace disabled"
    exit 0
fi

if grep -F "libsixel: abort() detected" "${log_file}" >/dev/null \
        && grep -F "libsixel: abort trace complete" "${log_file}" \
            >/dev/null; then
    echo "ok 1 - abort trace emitted"
else
    echo "not ok 1 - abort trace missing"
    sed 's/^/# /' "${log_file}"
    exit 1
fi
