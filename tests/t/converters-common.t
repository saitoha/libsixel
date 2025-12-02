#!/bin/sh
# Common helpers for converter TAP tests executed with POSIX sh.

set -u

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
parent_dir=$(CDPATH=; cd "${script_dir}/.." && pwd)

top_srcdir=${TOP_SRCDIR:-${parent_dir}}
top_builddir=${TOP_BUILDDIR:-${parent_dir}}

images_dir="${top_srcdir}/images"

if [ -z "${IMG2SIXEL_PATH:-}" ]; then
    if [ -x "${top_builddir}/converters/img2sixel" ]; then
        IMG2SIXEL_PATH="${top_builddir}/converters/img2sixel"
    else
        IMG2SIXEL_PATH="${top_srcdir}/converters/img2sixel"
    fi
fi

if [ -z "${SIXEL2PNG_PATH:-}" ]; then
    if [ -x "${top_builddir}/converters/sixel2png" ]; then
        SIXEL2PNG_PATH="${top_builddir}/converters/sixel2png"
    else
        SIXEL2PNG_PATH="${top_srcdir}/converters/sixel2png"
    fi
fi

wine_exec() {
    if [ -n "${WINE:-}" ]; then
        "${WINE}" "$@"
    else
        "$@"
    fi
}

run_img2sixel() {
    wine_exec "${IMG2SIXEL_PATH}" "$@"
}

run_sixel2png() {
    wine_exec "${SIXEL2PNG_PATH}" "$@"
}

make_temp_file() {
    dir_path=$1
    prefix=$2

    if command -v mktemp >/dev/null 2>&1; then
        mktemp "${dir_path}/${prefix}.XXXXXX"
        return 0
    fi

    candidate="${dir_path}/${prefix}.$$"
    : >"${candidate}"
    echo "${candidate}"
    return 0
}

require_file() {
    if [ ! -e "$1" ]; then
        echo "Required file '$1' is missing" >&2
        exit 1
    fi
}

export SIXEL_THREADS=${SIXEL_THREADS:-1}
