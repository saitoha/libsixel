#!/usr/bin/env bash
# Shared helpers for converter tests.
set -euo pipefail

normalize_path() {
    local path
    local drive
    local rest

    path=$1

    if [[ ${path} =~ ^\\\\ ]]; then
        rest=${path#\\\\}
        rest=${rest//\\//}
        printf '//%s' "${rest}"
        return 0
    fi

    if [[ ${path} =~ ^[A-Za-z]:[\\/].* ]]; then
        if command -v cygpath >/dev/null 2>&1; then
            cygpath -u "${path}"
            return 0
        fi
        drive=${path:0:1}
        rest=${path:2}
        rest=${rest//\\//}
        rest=${rest#/}
        drive=${drive,,}
        if [[ -n ${rest} ]]; then
            printf '/%s/%s' "${drive}" "${rest}"
        else
            printf '/%s' "${drive}"
        fi
        return 0
    fi

    printf '%s' "${path}"
    return 0
}

if [[ -z "${TOP_SRCDIR:-}" ]]; then
    echo "TOP_SRCDIR is not set" >&2
    exit 1
fi
if [[ -z "${SRC_DIR:-}" ]]; then
    echo "SRC_DIR is not set" >&2
    exit 1
fi
if [[ -z "${BUILD_DIR:-}" ]]; then
    echo "BUILD_DIR is not set" >&2
    exit 1
fi

TOP_SRCDIR=$(normalize_path "${TOP_SRCDIR}")
SRC_DIR=$(normalize_path "${SRC_DIR}")
BUILD_DIR=$(normalize_path "${BUILD_DIR}")
BUILD_DIR_ABS=$(cd "${BUILD_DIR}" && pwd)
TMP_DIR=${TMP_DIR:-"${BUILD_DIR}/tmp"}
TMP_DIR=$(normalize_path "${TMP_DIR}")
if [[ ${TMP_DIR} != /* ]]; then
    TMP_DIR="${BUILD_DIR_ABS}/${TMP_DIR}"
fi
WINE=${WINE:-}
WINEEXT=${WINEEXT:-}
IMG2SIXEL_PATH="${BUILD_DIR_ABS}/img2sixel${WINEEXT}"
SIXEL2PNG_PATH="${BUILD_DIR_ABS}/sixel2png${WINEEXT}"

IMAGES_DIR="${TOP_SRCDIR}/images"

mkdir -p "${TMP_DIR}"

wine_exec() {
    echo "$@" >&2
    if [[ -n "${WINE}" ]]; then
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

require_file() {
    if [[ ! -e "$1" ]]; then
        echo "Required file '$1' is missing" >&2
        exit 1
    fi
}
