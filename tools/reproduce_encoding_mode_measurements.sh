#!/bin/sh
# Rebuild and reproduce the encode-policy and high-color measurements.

set -eu

TOP_SRCDIR=${TOP_SRCDIR-${0%/*}/..}
TOP_SRCDIR=$(CDPATH='' cd -- "${TOP_SRCDIR}" && pwd -P)
BUILD_DIR=${BUILD_DIR-${TOP_SRCDIR}}
BUILD_DIR=$(CDPATH='' cd -- "${BUILD_DIR}" && pwd -P)
PYTHON=${PYTHON-python3}
MAKE=${MAKE-make}
GIT=${GIT-git}
IMG2SIXEL_PATH=${IMG2SIXEL_PATH-${BUILD_DIR}/converters/img2sixel}
SIXEL2PNG_PATH=${SIXEL2PNG_PATH-${BUILD_DIR}/converters/sixel2png}
LSQA_PATH=${LSQA_PATH-${BUILD_DIR}/assessment/lsqa}

encode_dir=${1-${TOP_SRCDIR}/docs/functionality/encode-policies/measurements}
high_dir=${2-${TOP_SRCDIR}/docs/functionality/high-color/measurements}
input_image=${3-images/snake.png}
warmups=${ENCODING_MODE_WARMUPS-2}
runs=${ENCODING_MODE_RUNS-7}
source_state=clean

"${GIT}" -C "${TOP_SRCDIR}" diff --quiet -- || source_state=dirty
"${GIT}" -C "${TOP_SRCDIR}" diff --cached --quiet -- || source_state=dirty
test "${source_state}" = clean || {
    echo "refusing to record measurements from a dirty tracked worktree" >&2
    echo "commit the implementation before producing durable measurements" >&2
    exit 1
}

revision=$("${GIT}" -C "${TOP_SRCDIR}" rev-parse HEAD)
PATH="${TOP_SRCDIR}/.local/bin:${PATH}"
LC_ALL=C
TZ=UTC
export PATH LC_ALL TZ
cd "${TOP_SRCDIR}"

"${MAKE}" -C "${BUILD_DIR}" all

"${PYTHON}" "${TOP_SRCDIR}/tools/plot_encoding_mode_measurements.py" \
    "${input_image}" \
    --img2sixel "${IMG2SIXEL_PATH}" \
    --sixel2png "${SIXEL2PNG_PATH}" \
    --lsqa "${LSQA_PATH}" \
    --revision "${revision}" \
    --source-state "${source_state}" \
    --warmups "${warmups}" \
    --runs "${runs}" \
    --encode-output-dir "${encode_dir}" \
    --high-color-output-dir "${high_dir}"

"${PYTHON}" "${TOP_SRCDIR}/tools/check_encoding_mode_measurements.py" \
    "${encode_dir}" "${high_dir}"
