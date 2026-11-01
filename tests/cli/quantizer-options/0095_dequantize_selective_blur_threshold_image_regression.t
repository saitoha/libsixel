#!/bin/sh
# Verify selective_blur:threshold preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DEQUANTIZE|g_dequantize_values + SIXEL_DEQUANTIZE_BASE_SELECTIVE_BLUR|threshold
# Registry binding: selective_blur_threshold

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/images/map8.six"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0002-dequantize-selective_blur-threshold-short-$$.png"
env_output="${artifact_dir}/0002-dequantize-selective_blur-threshold-env-$$.png"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    "-dselective_blur:T36" <"${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "selective_blur:threshold short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=threshold\|stored=1\|binding=selective_blur_threshold**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threshold short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env "SIXEL_DEQUANTIZE_SELECTIVE_BLUR_THRESHOLD=36" "-dselective_blur" \
    <"${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "selective_blur:threshold env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=threshold\|stored=1\|binding=selective_blur_threshold**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threshold environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "selective_blur:threshold short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "selective_blur:threshold preserves image output"
exit 0
