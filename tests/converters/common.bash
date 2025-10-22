#!/usr/bin/env bash
# Shared helpers for converter tests.
set -euo pipefail

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

BUILD_DIR_ABS=$(cd "${BUILD_DIR}" && pwd)
TMP_DIR=${TMP_DIR:-"${BUILD_DIR}/tmp"}
if [[ ${TMP_DIR} != /* ]]; then
    TMP_DIR="${BUILD_DIR_ABS}/${TMP_DIR}"
fi
WINE=${WINE:-}
WINEEXT=${WINEEXT:-}
IMG2SIXEL_PATH="${BUILD_DIR_ABS}/img2sixel${WINEEXT}"
SIXEL2PNG_PATH="${BUILD_DIR_ABS}/sixel2png${WINEEXT}"

CONVERTER_IMAGES_DIR="${SRC_DIR}/images"
if [[ -d "${CONVERTER_IMAGES_DIR}" ]]; then
    IMAGES_DIR="${CONVERTER_IMAGES_DIR}"
else
    IMAGES_DIR="${TOP_SRCDIR}/images"
fi

mkdir -p "${TMP_DIR}"

wine_exec() {
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
