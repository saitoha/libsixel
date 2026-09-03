#!/bin/sh
# Rebuild and reproduce every checked-in lookup-policy measurement artifact.

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

output_dir=${1-${TOP_SRCDIR}/docs/functionality/lookup-policies/measurements}
input_image=${2-images/snake.png}
warmups=${LOOKUP_POLICY_WARMUPS-2}
runs=${LOOKUP_POLICY_RUNS-9}
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

IMG2SIXEL_PATH=${IMG2SIXEL_PATH} \
LSQA_PATH=${LSQA_PATH} \
PYTHON=${PYTHON} \
    "${TOP_SRCDIR}/tools/plot_lookup_policy_quality.sh" \
    "${output_dir}" "${input_image}"

"${PYTHON}" "${TOP_SRCDIR}/tools/plot_lookup_policy_speed.py" \
    "${input_image}" \
    --img2sixel "${IMG2SIXEL_PATH}" \
    --lsqa "${LSQA_PATH}" \
    --build-dir "${BUILD_DIR}" \
    --revision "${revision}" \
    --source-state "${source_state}" \
    --clean-sixel-environment \
    --warmups "${warmups}" \
    --runs "${runs}" \
    --output-csv "${output_dir}/lookup-policy-speed.csv" \
    --output-plot "${output_dir}/lookup-policy-speed.png" \
    --output-metadata "${output_dir}/lookup-policy-run.json"

"${PYTHON}" "${TOP_SRCDIR}/tools/plot_lookup_policy_speed.py" \
    "${input_image}" \
    --img2sixel "${IMG2SIXEL_PATH}" \
    --lsqa "${LSQA_PATH}" \
    --build-dir "${BUILD_DIR}" \
    --revision "${revision}" \
    --source-state "${source_state}" \
    --clean-sixel-environment \
    --diffusion fs \
    --warmups "${warmups}" \
    --runs "${runs}" \
    --output-csv "${output_dir}/lookup-policy-fs-speed.csv" \
    --output-plot "${output_dir}/lookup-policy-fs-speed.png" \
    --output-metadata "${output_dir}/lookup-policy-fs-run.json" \
    --title "End-to-end lookup-policy runtime with FS on ${input_image##*/}"

"${PYTHON}" "${TOP_SRCDIR}/tools/check_lookup_policy_measurements.py" \
    "${output_dir}"
