#!/bin/sh
# Rebuild and reproduce the static dither-spectrum measurement artifacts.

set -eu

TOP_SRCDIR=${TOP_SRCDIR-${0%/*}/..}
TOP_SRCDIR=$(CDPATH='' cd -- "${TOP_SRCDIR}" && pwd -P)
BUILD_DIR=${BUILD_DIR-${TOP_SRCDIR}}
BUILD_DIR=$(CDPATH='' cd -- "${BUILD_DIR}" && pwd -P)
PYTHON=${PYTHON-python3}
MAKE=${MAKE-make}
GIT=${GIT-git}
IMG2SIXEL_PATH=${IMG2SIXEL_PATH-${BUILD_DIR}/converters/img2sixel}

output_dir=${1-${TOP_SRCDIR}/docs/functionality/dither-policies/measurements}
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

"${PYTHON}" "${TOP_SRCDIR}/tools/plot_dither_spectrum_measurements.py" \
    --img2sixel "${IMG2SIXEL_PATH}" \
    --build-dir "${BUILD_DIR}" \
    --revision "${revision}" \
    --source-state "${source_state}" \
    --clean-sixel-environment \
    --output-summary-csv "${output_dir}/dither-spectrum-summary.csv" \
    --output-curves-csv "${output_dir}/dither-spectrum-curves.csv" \
    --output-atlas "${output_dir}/dither-policy-spectrum-atlas.png" \
    --output-overview "${output_dir}/dither-policy-spectrum-overview.png" \
    --output-metadata "${output_dir}/dither-spectrum-run.json"

"${PYTHON}" "${TOP_SRCDIR}/tools/check_dither_spectrum_measurements.py" \
    "${output_dir}"
