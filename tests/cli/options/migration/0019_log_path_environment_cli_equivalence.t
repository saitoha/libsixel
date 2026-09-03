#!/bin/sh
# Verify the timeline log path option matches its environment variable.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
short_output="${ARTIFACT_LOCAL_DIR}/0019-log-path-short-$$.six"
env_output="${ARTIFACT_LOCAL_DIR}/0019-log-path-env-$$.six"
short_log="${ARTIFACT_LOCAL_DIR}/0019-log-path-short-$$.jsonl"
env_log="${ARTIFACT_LOCAL_DIR}/0019-log-path-env-$$.jsonl"
shadow_log="${ARTIFACT_LOCAL_DIR}/0019-log-path-shadow-$$.jsonl"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOG_PATH=${shadow_log}" -J "${short_log}" \
    "${input_image}" >"${short_output}" || {
    echo "not ok" 1 - "log path option conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOG_PATH=${env_log}" \
    "${input_image}" >"${env_output}" || {
    echo "not ok" 1 - "log path environment conversion failed"
    exit 0
}

test -s "${short_log}" || {
    echo "not ok" 1 - "log path option did not reach the timeline writer"
    exit 0
}
test ! -e "${shadow_log}" || {
    echo "not ok" 1 - "log path environment overrode the option"
    exit 0
}
test -s "${env_log}" || {
    echo "not ok" 1 - "log path environment did not reach the timeline writer"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "log path option and environment outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "log path image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "log path option matches environment output"
exit 0
