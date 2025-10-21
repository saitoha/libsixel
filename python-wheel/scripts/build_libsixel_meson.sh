#!/usr/bin/env bash
set -euxo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SRC_DIR="${LIBSIXEL_SRC:-${REPO_ROOT}}"
BUILD_DIR="${SRC_DIR}/python-wheel/.build-meson"
STAGE_DIR="${SRC_DIR}/python-wheel/.stage"

python -m pip install --upgrade meson ninja

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
rm -rf "${STAGE_DIR}"
mkdir -p "${STAGE_DIR}"

meson setup "${BUILD_DIR}" "${SRC_DIR}" \
  --prefix "${STAGE_DIR}" \
  -Ddefault_library=shared \
  -Dpython=disabled \
  -Dgdk_pixbuf2=disabled \
  -Dexamples=disabled \
  -Dtests=disabled
meson compile -C "${BUILD_DIR}"
meson install -C "${BUILD_DIR}"
