#!/bin/sh
# Verify sixel2png accepts the shared timeline log path option.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_six="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
short_output="${ARTIFACT_LOCAL_DIR}/0009-log-path-short-$$.png"
env_output="${ARTIFACT_LOCAL_DIR}/0009-log-path-env-$$.png"
short_log="${ARTIFACT_LOCAL_DIR}/0009-log-path-short-$$.jsonl"
env_log="${ARTIFACT_LOCAL_DIR}/0009-log-path-env-$$.jsonl"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -J "${short_log}" \
    -i "${input_six}" -o "${short_output}" || {
    echo "not ok" 1 - "sixel2png log path option failed"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env "SIXEL_LOG_PATH=${env_log}" \
    -i "${input_six}" -o "${env_output}" || {
    echo "not ok" 1 - "sixel2png log path environment failed"
    exit 0
}

test -s "${short_log}" || {
    echo "not ok" 1 - "sixel2png option did not reach the timeline writer"
    exit 0
}
test -s "${env_log}" || {
    echo "not ok" 1 - "sixel2png environment did not reach the timeline writer"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "sixel2png log path outputs differ"
    exit 0
}

echo "ok" 1 - "sixel2png log path option matches environment output"
exit 0
