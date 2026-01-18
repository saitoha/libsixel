#!/bin/sh
# Common helpers for palette k-means init tests.

set -eu

locate_kmeans_binary() {
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

    printf '%s\n' "${binary}"
}

run_kmeans_init() {
    env_value=$1
    shift

    binary=$(locate_kmeans_binary)
    SIXEL_PALETTE_KMEANS_INITTYPE="${env_value}" \
        "${binary}" "palette/0001_kmeans_init" "$@" | tr -d '\r'
}
