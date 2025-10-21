#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <stage-dir>" >&2
    exit 1
fi

STAGE_DIR="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
TARGET_DIR="${REPO_ROOT}/python-wheel/src/libsixel_wheel/.binaries"

if [ ! -d "${STAGE_DIR}" ]; then
    echo "stage directory not found: ${STAGE_DIR}" >&2
    exit 1
fi

mkdir -p "${TARGET_DIR}"
rm -f "${TARGET_DIR}"/*

FOUND=0
copy_matches() {
    local pattern="$1"
    while IFS= read -r -d '' artifact; do
        cp "${artifact}" "${TARGET_DIR}/"
        FOUND=1
    done < <(find "${STAGE_DIR}" -type f -name "${pattern}" -print0)
}

copy_matches "libsixel*.so"
copy_matches "libsixel*.so.*"
copy_matches "libsixel*.dylib"
copy_matches "libsixel*.dll"

if [ "${FOUND}" -eq 0 ]; then
    echo "no libsixel shared libraries found in ${STAGE_DIR}" >&2
    exit 1
fi
