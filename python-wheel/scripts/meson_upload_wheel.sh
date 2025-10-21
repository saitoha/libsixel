#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
WHEELHOUSE_DIR="${REPO_ROOT}/python-wheel/wheelhouse"

REPOSITORY="${PYPI_REPO:-pypi}"
USERNAME="${PYPI_USERNAME:-__token__}"
PASSWORD_ENV="${PYPI_PASSWORD_ENV:-TWINE_PASSWORD}"
PASSWORD=""

if [ -z "${PASSWORD_ENV}" ]; then
    echo "PYPI_PASSWORD_ENV is empty" >&2
    exit 1
fi
PASSWORD="$(printenv "${PASSWORD_ENV}" || true)"
if [ -z "${PASSWORD}" ]; then
    echo "environment variable ${PASSWORD_ENV} is not set" >&2
    exit 1
fi

if [ ! -d "${WHEELHOUSE_DIR}" ]; then
    echo "wheelhouse directory not found: ${WHEELHOUSE_DIR}" >&2
    exit 1
fi

twine upload \
  --repository "${REPOSITORY}" \
  -u "${USERNAME}" \
  -p "${PASSWORD}" \
  "${WHEELHOUSE_DIR}"/*.whl
