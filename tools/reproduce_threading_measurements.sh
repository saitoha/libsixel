#!/bin/sh
# Rebuild and reproduce thread-budget, scaling, and timeline artifacts.

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

output_dir=${1-${TOP_SRCDIR}/docs/threading/measurements}
static_input=${2-${TOP_SRCDIR}/images/measurements/palette-pipeline/smooth-gradient-900x675.png}
animation_input=${3-${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-no-netscape-2frame.gif}
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

test -d "${output_dir}" || mkdir -p "${output_dir}"
"${MAKE}" -C "${BUILD_DIR}" all

"${PYTHON}" "${TOP_SRCDIR}/tools/plot_threading_measurements.py" \
    --img2sixel "${IMG2SIXEL_PATH}" \
    --sixel2png "${SIXEL2PNG_PATH}" \
    --build-dir "${BUILD_DIR}" \
    --input "${static_input}" \
    --animation-input "${animation_input}" \
    --revision "${revision}" \
    --source-state "${source_state}" \
    --clean-sixel-environment \
    --output-dir "${output_dir}"

for threads in 2 4 8; do
    "${PYTHON}" "${TOP_SRCDIR}/tools/timeline.py" \
        --sort-order start \
        --frame-mode off \
        "${output_dir}/encoder-thread${threads}.jsonl" \
        --output "${output_dir}/encoder-thread${threads}-timeline.png"
done

"${PYTHON}" "${TOP_SRCDIR}/tools/timeline.py" \
    --sort-order start \
    --frame-mode off \
    "${output_dir}/decoder-thread8.jsonl" \
    --output "${output_dir}/decoder-thread8-timeline.png"

"${PYTHON}" "${TOP_SRCDIR}/tools/timeline.py" \
    --sort-order start \
    --frame-mode on \
    "${output_dir}/animation-thread4.jsonl" \
    --output "${output_dir}/animation-thread4-timeline.png"

"${PYTHON}" "${TOP_SRCDIR}/tools/check_threading_measurements.py" \
    "${output_dir}"
