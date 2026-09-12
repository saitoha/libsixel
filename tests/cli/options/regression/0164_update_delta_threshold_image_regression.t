#!/bin/sh
# Verify delta:threshold stores the same setting via short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_UPDATE_POLICY|g_update_policy_values + 1|threshold
# Registry binding: sixdelta_threshold|sixdelta_threshold_override

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0164_update_delta_threshold_image_regression-short-$$.six"
env_output="${artifact_dir}/0164_update_delta_threshold_image_regression-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Zdelta:T8" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok 1 - delta:threshold short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=threshold|stored=1|binding=sixdelta_threshold,sixdelta_threshold_override|value=8*}" != "${short_trace}" || {
    echo "not ok 1 - delta:threshold short value was not stored"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_UPDATE_DELTA_THRESHOLD=8" -Z delta \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok 1 - delta:threshold environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=threshold|stored=1|binding=sixdelta_threshold,sixdelta_threshold_override|value=8*}" != "${env_trace}" || {
    echo "not ok 1 - delta:threshold environment value was not stored"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - delta:threshold output differs"
    exit 0
}
${SIXEL_RUNTIME-} "${LSQA_PATH}" -b MS-SSIM:0.98 \
    "${input_image}" "${short_output}" || {
    echo "not ok 1 - delta:threshold image quality regressed"
    exit 0
}
echo "ok 1 - delta:threshold preserves image output"
exit 0
