#!/usr/bin/env bash
set -euxo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SRC_DIR="${LIBSIXEL_SRC:-${REPO_ROOT}}"
BUILD_DIR="${SRC_DIR}/python-wheel/.build-autotools"
STAGE_DIR="${SRC_DIR}/python-wheel/.stage"
JOBS="${LIBSIXEL_JOBS:-}"

if [ -z "${JOBS}" ]; then
    if command -v nproc >/dev/null 2>&1; then
        JOBS="$(nproc)"
    elif command -v sysctl >/dev/null 2>&1; then
        JOBS="$(sysctl -n hw.ncpu)"
    else
        JOBS=2
    fi
fi

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
rm -rf "${STAGE_DIR}"
mkdir -p "${STAGE_DIR}"

cd "${BUILD_DIR}"
"${SRC_DIR}/configure" \
  --prefix="${STAGE_DIR}" \
  --enable-shared \
  --disable-static \
  --without-gdk-pixbuf2 \
  --disable-python \
  --disable-examples \
  --disable-tests
make -j"${JOBS}"
make install
