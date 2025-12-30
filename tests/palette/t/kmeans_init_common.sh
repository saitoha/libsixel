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

    binary="${top_builddir}/tests/0001_kmeans_init"
    if [ ! -x "${binary}" ]; then
        alt_binary="${top_builddir}/tests/palette/0001_kmeans_init"
        if [ -x "${alt_binary}" ]; then
            binary="${alt_binary}"
        else
            echo "harness not built" >&2
            exit 99
        fi
    fi

    printf '%s\n' "${binary}"
}

run_kmeans_init() {
    env_value=$1
    shift

    binary=$(locate_kmeans_binary)
    SIXEL_PALETTE_KMEANS_INITTYPE="${env_value}" \
        "${binary}" "$@" | tr -d '\r'
}
