#!/bin/sh
# Rebuild and reproduce the working-color-space comparison artifacts.

set -eu

TOP_SRCDIR=${TOP_SRCDIR-${0%/*}/..}
TOP_SRCDIR=$(CDPATH='' cd -- "${TOP_SRCDIR}" && pwd -P)
BUILD_DIR=${BUILD_DIR-${TOP_SRCDIR}}
BUILD_DIR=$(CDPATH='' cd -- "${BUILD_DIR}" && pwd -P)
PYTHON=${PYTHON-python3}
MAKE=${MAKE-make}
GIT=${GIT-git}
IMG2SIXEL_PATH=${IMG2SIXEL_PATH-${BUILD_DIR}/converters/img2sixel}
LSQA_PATH=${LSQA_PATH-${BUILD_DIR}/assessment/lsqa}

output_dir=${1-${TOP_SRCDIR}/docs/functionality/working-colorspaces/measurements}
input_image=${2-images/snake.png}
warmups=${WORKING_COLORSPACE_WARMUPS-2}
runs=${WORKING_COLORSPACE_RUNS-7}
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

"${PYTHON}" "${TOP_SRCDIR}/tools/plot_working_colorspace_measurements.py" \
    "${input_image}" \
    --img2sixel "${IMG2SIXEL_PATH}" \
    --lsqa "${LSQA_PATH}" \
    --build-dir "${BUILD_DIR}" \
    --revision "${revision}" \
    --source-state "${source_state}" \
    --clean-sixel-environment \
    --warmups "${warmups}" \
    --runs "${runs}" \
    --output-csv "${output_dir}/working-colorspace-comparison.csv" \
    --output-quality-plot "${output_dir}/working-colorspace-quality.png" \
    --output-performance-plot \
    "${output_dir}/working-colorspace-performance.png" \
    --output-metadata "${output_dir}/working-colorspace-run.json"

"${PYTHON}" \
    "${TOP_SRCDIR}/tools/check_working_colorspace_measurements.py" \
    "${output_dir}"
