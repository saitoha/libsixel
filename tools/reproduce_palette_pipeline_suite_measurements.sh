#!/bin/sh
# Rebuild and reproduce the multi-fixture palette-pipeline measurement suite.

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

output_dir=${1-${TOP_SRCDIR}/docs/functionality/palette-pipeline/suite-measurements}
manifest=${2-${TOP_SRCDIR}/tools/palette_pipeline_suite.json}
warmups=${PALETTE_PIPELINE_SUITE_WARMUPS-2}
runs=${PALETTE_PIPELINE_SUITE_RUNS-10}
exploratory=${PALETTE_PIPELINE_SUITE_EXPLORATORY-0}
measurement_mode=durable
source_state=clean

cd "${TOP_SRCDIR}"
test -n "${output_dir}" || {
    echo "output directory must not be empty" >&2
    exit 1
}
output_dir=$("${PYTHON}" -c \
    'from pathlib import Path; import sys; print(Path(sys.argv[1]).resolve())' \
    "${output_dir}")
durable_output_dir=$("${PYTHON}" -c \
    'from pathlib import Path; import sys; print(Path(sys.argv[1]).resolve())' \
    "${TOP_SRCDIR}/docs/functionality/palette-pipeline/suite-measurements")
manifest=$("${PYTHON}" -c \
    'from pathlib import Path; import sys; print(Path(sys.argv[1]).resolve())' \
    "${manifest}")

test "${exploratory}" = 0 || test "${exploratory}" = 1 || {
    echo "PALETTE_PIPELINE_SUITE_EXPLORATORY must be 0 or 1" >&2
    exit 1
}
test "${exploratory}" = 0 || measurement_mode=exploratory
test "${measurement_mode}" = durable || test "$#" -ge 1 || {
    echo "exploratory suite measurements require an explicit output directory" >&2
    exit 1
}
test "${measurement_mode}" = durable || \
    test "${output_dir}" != "${durable_output_dir}" || {
    echo "exploratory suite cannot use the durable output directory" >&2
    exit 1
}
test "${measurement_mode}" = exploratory || test "${warmups}" = 2 || {
    echo "durable suite measurements require exactly 2 warm-ups" >&2
    exit 1
}
test "${measurement_mode}" = exploratory || test "${runs}" = 10 || {
    echo "durable suite measurements require exactly 10 recorded runs" >&2
    exit 1
}

"${GIT}" -C "${TOP_SRCDIR}" diff --quiet -- || source_state=dirty
"${GIT}" -C "${TOP_SRCDIR}" diff --cached --quiet -- || source_state=dirty
test "${measurement_mode}" = exploratory || test "${source_state}" = clean || {
    echo "refusing to record a durable suite from a dirty tracked worktree" >&2
    echo "commit the suite implementation before recording measurements" >&2
    exit 1
}

revision=$("${GIT}" -C "${TOP_SRCDIR}" rev-parse HEAD)
PATH="${TOP_SRCDIR}/.local/bin:${PATH}"
LC_ALL=C
TZ=UTC
export PATH LC_ALL TZ GIT

test -d "${output_dir}" || mkdir -p "${output_dir}"
"${MAKE}" -C "${BUILD_DIR}" all
"${PYTHON}" \
    "${TOP_SRCDIR}/tools/generate_palette_pipeline_suite_fixtures.py" \
    --check

"${PYTHON}" \
    "${TOP_SRCDIR}/tools/plot_palette_pipeline_suite_measurements.py" \
    --manifest "${manifest}" \
    --output-dir "${output_dir}" \
    --img2sixel "${IMG2SIXEL_PATH}" \
    --lsqa "${LSQA_PATH}" \
    --build-dir "${BUILD_DIR}" \
    --revision "${revision}" \
    --source-state "${source_state}" \
    --measurement-mode "${measurement_mode}" \
    --warmups "${warmups}" \
    --runs "${runs}"

"${PYTHON}" \
    "${TOP_SRCDIR}/tools/check_palette_pipeline_suite_measurements.py" \
    "${output_dir}" \
    --mode "${measurement_mode}"
