#!/bin/sh
# TAP test covering cli_token_is_known_option with short/long tokens.

set -euxv

name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${name}"
mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
parent_dir=$(CDPATH=; cd "${script_dir}/../.." && pwd)

if [ -n "${MESON_BUILD_ROOT:-}" ]; then
    top_builddir=${TOP_BUILDDIR:-${MESON_BUILD_ROOT}}
else
    top_builddir=${TOP_BUILDDIR:-${parent_dir}}
fi

binary="${top_builddir}/tests/test_runner"
if [ ! -x "${binary}" ]; then
    echo "harness not built" >&2
    exit 99
fi

log_file="${artifact_dir}/test.log"
set +e
"${binary}" "cli/0029_cli_token_is_known_option" >"${log_file}" 2>&1
rc=$?
set -e

echo "1..1"

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - cli_token_is_known_option"
else
    echo "not ok 1 - cli_token_is_known_option"
    sed 's/^/# /' "${log_file}"
    exit 1
fi
