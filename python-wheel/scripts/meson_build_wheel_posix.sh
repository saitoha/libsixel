#!/usr/bin/env bash
set -euxo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
WHEEL_DIR="${REPO_ROOT}/python-wheel"
STAGE_DIR="${WHEEL_DIR}/.stage"
DIST_DIR="${WHEEL_DIR}/dist"
WHEELHOUSE_DIR="${WHEEL_DIR}/wheelhouse"

if [ ! -d "${STAGE_DIR}" ]; then
    echo "stage directory not found: ${STAGE_DIR}" >&2
    exit 1
fi

"${SCRIPT_DIR}/copy_libsixel_artifacts.sh" "${STAGE_DIR}"

rm -rf "${DIST_DIR}" "${WHEELHOUSE_DIR}"
mkdir -p "${DIST_DIR}" "${WHEELHOUSE_DIR}"

python -m build --wheel --outdir "${DIST_DIR}" "${WHEEL_DIR}"
WHEEL_PATH="$(ls "${DIST_DIR}"/libsixel_wheel-*.whl | head -n1)"
if [ -z "${WHEEL_PATH}" ]; then
    echo "wheel build did not produce an artifact" >&2
    exit 1
fi

UNAME="$(uname)"
if [ "${UNAME}" = "Linux" ]; then
    auditwheel -v repair -w "${WHEELHOUSE_DIR}" "${WHEEL_PATH}"
elif [ "${UNAME}" = "Darwin" ]; then
    delocate-wheel -v -w "${WHEELHOUSE_DIR}" "${WHEEL_PATH}"
else
    cp "${WHEEL_PATH}" "${WHEELHOUSE_DIR}/"
fi
