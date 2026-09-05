#!/bin/sh
# Rebuild and reproduce the palette sampling and binning comparison artifacts.

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

output_dir=${1-${TOP_SRCDIR}/docs/functionality/palette-pipeline/measurements}
input_image=${2-images/snake.png}
warmups=${PALETTE_PIPELINE_WARMUPS-2}
runs=${PALETTE_PIPELINE_RUNS-10}
exploratory=${PALETTE_PIPELINE_EXPLORATORY-0}
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
    "${TOP_SRCDIR}/docs/functionality/palette-pipeline/measurements")

test "${exploratory}" = 0 || test "${exploratory}" = 1 || {
    echo "PALETTE_PIPELINE_EXPLORATORY must be 0 or 1" >&2
    exit 1
}
test "${exploratory}" = 0 || measurement_mode=exploratory
test "${measurement_mode}" = durable || test "$#" -ge 1 || {
    echo "exploratory measurements require an explicit output directory" >&2
    exit 1
}
test "${measurement_mode}" = durable || \
    test "${output_dir}" != "${durable_output_dir}" || {
    echo "exploratory measurements cannot use the durable output directory" >&2
    exit 1
}
test "${measurement_mode}" = exploratory || test "${warmups}" = 2 || {
    echo "durable measurements require exactly 2 warm-ups" >&2
    exit 1
}
test "${measurement_mode}" = exploratory || test "${runs}" = 10 || {
    echo "durable measurements require exactly 10 recorded runs" >&2
    exit 1
}

"${GIT}" -C "${TOP_SRCDIR}" diff --quiet -- || source_state=dirty
"${GIT}" -C "${TOP_SRCDIR}" diff --cached --quiet -- || source_state=dirty
test "${measurement_mode}" = exploratory || test "${source_state}" = clean || {
    echo "refusing to record measurements from a dirty tracked worktree" >&2
    echo "commit the implementation before producing durable measurements" >&2
    exit 1
}

revision=$("${GIT}" -C "${TOP_SRCDIR}" rev-parse HEAD)
PATH="${TOP_SRCDIR}/.local/bin:${PATH}"
LC_ALL=C
TZ=UTC
export PATH LC_ALL TZ

test -d "${output_dir}" || mkdir -p "${output_dir}"
"${MAKE}" -C "${BUILD_DIR}" all

"${PYTHON}" "${TOP_SRCDIR}/tools/plot_palette_pipeline_measurements.py" \
    "${input_image}" \
    --img2sixel "${IMG2SIXEL_PATH}" \
    --lsqa "${LSQA_PATH}" \
    --build-dir "${BUILD_DIR}" \
    --revision "${revision}" \
    --source-state "${source_state}" \
    --measurement-mode "${measurement_mode}" \
    --clean-sixel-environment \
    --warmups "${warmups}" \
    --runs "${runs}" \
    --output-quality-csv "${output_dir}/palette-pipeline-quality.csv" \
    --output-quality-plot "${output_dir}/palette-pipeline-quality.png" \
    --output-size-csv "${output_dir}/palette-pipeline-size.csv" \
    --output-size-plot "${output_dir}/palette-pipeline-size.png" \
    --output-speed-csv "${output_dir}/palette-pipeline-speed.csv" \
    --output-speed-plot "${output_dir}/palette-pipeline-speed.png" \
    --output-metadata "${output_dir}/palette-pipeline-run.json"

"${PYTHON}" "${TOP_SRCDIR}/tools/check_palette_pipeline_measurements.py" \
    "${output_dir}" \
    --mode "${measurement_mode}"
