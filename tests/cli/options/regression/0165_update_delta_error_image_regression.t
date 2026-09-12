#!/bin/sh
# Verify delta:error stores the same setting via short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_UPDATE_POLICY|g_update_policy_values + 1|error
# Registry binding: sixdelta_error_mode|sixdelta_error_mode_override

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
short_output="${artifact_dir}/0165_update_delta_error_image_regression-short-$$.six"
env_output="${artifact_dir}/0165_update_delta_error_image_regression-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Zdelta:Eskip" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok 1 - delta:error short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=error|stored=1|binding=sixdelta_error_mode,sixdelta_error_mode_override|value=1*}" != "${short_trace}" || {
    echo "not ok 1 - delta:error short value was not stored"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_UPDATE_DELTA_ERROR=skip" -Z delta \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok 1 - delta:error environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=error|stored=1|binding=sixdelta_error_mode,sixdelta_error_mode_override|value=1*}" != "${env_trace}" || {
    echo "not ok 1 - delta:error environment value was not stored"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - delta:error output differs"
    exit 0
}
${SIXEL_RUNTIME-} "${LSQA_PATH}" -b MS-SSIM:0.98 \
    "${input_image}" "${short_output}" || {
    echo "not ok 1 - delta:error image quality regressed"
    exit 0
}
echo "ok 1 - delta:error preserves image output"
exit 0
