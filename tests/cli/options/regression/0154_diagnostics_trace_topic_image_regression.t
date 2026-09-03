#!/bin/sh
# Verify trace topic selection through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIAGNOSTICS|NULL|trace_topic
# Registry binding: trace_topic|trace_topic_override

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0154-trace-topic-short-$$.six"
env_output="${artifact_dir}/0154-trace-topic-env-$$.six"

short_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_TRACE_TOPIC=suboption_contract" \
    "-xhuman:Tlifecycle" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "trace topic short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=trace_topic|stored=1|binding=trace_topic,trace_topic_override|value=lifecycle*}" != "${short_trace}" || {
    echo "not ok" 1 - "trace topic short value was not stored"
    exit 0
}

env_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_TRACE_TOPIC=suboption_contract,lifecycle" -xhuman \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "trace topic environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=trace_topic|stored=1|binding=trace_topic,trace_topic_override|value=suboption_contract,lifecycle*}" != "${env_trace}" || {
    echo "not ok" 1 - "trace topic environment value was not stored"
    exit 0
}

test "${short_trace#*img2sixel?lifecycle?:*}" != "${short_trace}" || {
    echo "not ok" 1 - "trace topic did not reach its consumer"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "trace topic outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "trace topic image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "trace topic preserves image and diagnostics output"
exit 0
