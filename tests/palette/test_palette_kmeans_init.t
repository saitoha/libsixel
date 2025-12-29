#!/bin/sh
# TAP runner validating SIXEL_PALETTE_KMEANS_INITTYPE handling.

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

binary="${top_builddir}/tests/test_palette_kmeans_init"
if [ ! -x "${binary}" ]; then
    alt_binary="${top_builddir}/tests/palette/test_palette_kmeans_init"
    if [ -x "${alt_binary}" ]; then
        binary="${alt_binary}"
    else
        echo "harness not built" >&2
        exit 99
    fi
fi

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
}

case_id=1
status=0

echo "1..5"

check_expected() {
    expected=$1
    env_value=$2

    output=$(SIXEL_PALETTE_KMEANS_INITTYPE="${env_value}" "${binary}" | tr -d '\r')
    if [ "${output}" = "${expected}" ]; then
        pass ${case_id} "${env_value:-unset} -> ${expected}"
    else
        fail ${case_id} "${env_value:-unset} -> ${output} (expected ${expected})"
        status=1
    fi
    case_id=$((case_id + 1))
}

check_expected "none" ""
check_expected "none" "auto"
check_expected "none" "unknown"
check_expected "pca" "pca"

cache_line=$(SIXEL_PALETTE_KMEANS_INITTYPE="pca" "${binary}" --cache | tr -d '\r')
if [ "${cache_line}" = "pca pca" ]; then
    pass ${case_id} "cache preserves initial value"
else
    fail ${case_id} "cache check returned '${cache_line}'"
    status=1
fi

exit "${status}"
