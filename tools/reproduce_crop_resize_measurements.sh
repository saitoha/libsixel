#!/bin/sh
# Rebuild and reproduce crop/resize documentation measurements and figures.

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

measurement_dir=${1-${TOP_SRCDIR}/docs/functionality/crop-resize/measurements}
policy_figure_dir=${2-${TOP_SRCDIR}/docs/functionality/crop-resize/figures}
resampling_figure_dir=${3-${TOP_SRCDIR}/docs/functionality/resampling/figures}
input_image=${4-${TOP_SRCDIR}/images/snake.png}
warmups=${CROP_RESIZE_WARMUPS-2}
runs=${CROP_RESIZE_RUNS-9}
source_state=clean

"${GIT}" -C "${TOP_SRCDIR}" diff --quiet -- || source_state=dirty
"${GIT}" -C "${TOP_SRCDIR}" diff --cached --quiet -- || source_state=dirty
test "${source_state}" = clean || {
    echo "refusing to record measurements from a dirty tracked worktree" >&2
    echo "commit the implementation before producing durable measurements" >&2
    exit 1
}

"${PYTHON}" -c 'import matplotlib, numpy, PIL' >/dev/null 2>&1 || {
    echo "crop/resize measurements require matplotlib, numpy, and Pillow" >&2
    exit 1
}

revision=$("${GIT}" -C "${TOP_SRCDIR}" rev-parse HEAD)
PATH="${TOP_SRCDIR}/.local/bin:${PATH}"
LC_ALL=C
TZ=UTC
export PATH LC_ALL TZ
cd "${TOP_SRCDIR}"

test -d "${measurement_dir}" || mkdir -p "${measurement_dir}"
test -d "${policy_figure_dir}" || mkdir -p "${policy_figure_dir}"
test -d "${resampling_figure_dir}" || mkdir -p "${resampling_figure_dir}"

"${MAKE}" -C "${BUILD_DIR}" all

"${PYTHON}" "${TOP_SRCDIR}/tools/generate_crop_resize_policy_path.py" \
    --output-directory "${policy_figure_dir}"

"${PYTHON}" "${TOP_SRCDIR}/tools/plot_crop_resize_measurements.py" \
    "${input_image}" \
    --img2sixel "${IMG2SIXEL_PATH}" \
    --sixel2png "${SIXEL2PNG_PATH}" \
    --lsqa "${LSQA_PATH}" \
    --revision "${revision}" \
    --source-state "${source_state}" \
    --warmups "${warmups}" \
    --runs "${runs}" \
    --output-directory "${measurement_dir}" \
    --resampling-figure-directory "${resampling_figure_dir}"

"${PYTHON}" "${TOP_SRCDIR}/tools/check_crop_resize_measurements.py" \
    "${measurement_dir}" \
    "${policy_figure_dir}" \
    "${resampling_figure_dir}"
