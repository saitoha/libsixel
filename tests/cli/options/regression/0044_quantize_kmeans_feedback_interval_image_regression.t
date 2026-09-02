#!/bin/sh
# Verify kmeans:feedback_interval preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS|feedback_interval
# Registry binding: quantize_model_kmeans_feedback_interval|quantize_model_kmeans_feedback_interval_override

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0044-quantize-kmeans-feedback_interval-short-$$.six"
env_output="${artifact_dir}/0044-quantize-kmeans-feedback_interval-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qkmeans:F1:A4:J2" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "kmeans:feedback_interval short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=feedback_interval|stored=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "feedback_interval short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEANS_FEEDBACK_INTERVAL=2" -p 16 "-Qkmeans:F1:A4" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "kmeans:feedback_interval env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=feedback_interval|stored=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "feedback_interval environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "kmeans:feedback_interval short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "kmeans:feedback_interval image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "kmeans:feedback_interval preserves image output"
exit 0
