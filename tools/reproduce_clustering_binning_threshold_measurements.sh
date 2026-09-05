#!/bin/sh
# Rebuild and reproduce the high-K hard-binning sufficiency artifacts.

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

output_dir=${1-${TOP_SRCDIR}/docs/functionality/clustering-color-spaces/threshold-measurements}
manifest=${2-${TOP_SRCDIR}/tools/palette_pipeline_suite.json}
colors=${CLUSTERING_BINNING_THRESHOLD_COLORS-64,128,256}
seeds=${CLUSTERING_BINNING_THRESHOLD_SEEDS-1,2,3,4,5}
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

"${PYTHON}" \
    "${TOP_SRCDIR}/tools/plot_clustering_binning_threshold_measurements.py" \
    --manifest "${manifest}" \
    --output-dir "${output_dir}" \
    --img2sixel "${IMG2SIXEL_PATH}" \
    --lsqa "${LSQA_PATH}" \
    --build-dir "${BUILD_DIR}" \
    --revision "${revision}" \
    --source-state "${source_state}" \
    --colors "${colors}" \
    --seeds "${seeds}" \
    --clean-sixel-environment

"${PYTHON}" \
    "${TOP_SRCDIR}/tools/check_clustering_binning_threshold_measurements.py" \
    "${output_dir}"
