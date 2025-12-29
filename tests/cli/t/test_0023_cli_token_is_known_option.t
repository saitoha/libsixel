#!/bin/sh
# TAP test covering cli_token_is_known_option with short/long tokens.

set -euxv

name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${name}"
mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
parent_dir=$(CDPATH=; cd "${script_dir}/.." && pwd)

if [ -n "${MESON_BUILD_ROOT:-}" ]; then
    top_builddir=${TOP_BUILDDIR:-${MESON_BUILD_ROOT}}
else
    top_builddir=${TOP_BUILDDIR:-${parent_dir}}
fi

binary="${top_builddir}/tests/test_0023_cli_token_is_known_option"
if [ ! -x "${binary}" ]; then
    alt_binary="${top_builddir}/tests/cli/test_0023_cli_token_is_known_option"
    if [ -x "${alt_binary}" ]; then
        binary="${alt_binary}"
    else
        echo "harness not built" >&2
        exit 99
    fi
fi

"${binary}" | tee "${artifact_dir}/tap.log"
