#!/bin/sh
# Verify timeline line sampling through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIAGNOSTICS|NULL|log_lines
# Registry binding: log_lines|log_lines_override
# Environment range: clamp-both
# Timeline contract: line_events=1|line_stride=3

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0155-log-lines-short-$$.six"
env_output="${artifact_dir}/0155-log-lines-env-$$.six"

short_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_TRACE_TOPIC=suboption_contract,timeline_contract" \
    --env "SIXEL_LOG_LINES=7" "-xhuman:N3" -~fhedt -p16 \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "log lines short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=log_lines|stored=1|binding=log_lines,log_lines_override|value=3*}" != "${short_trace}" || {
    echo "not ok" 1 - "log lines short value was not stored"
    exit 0
}
test "${short_trace#*LSXTLN1|*line_events=1|line_stride=3*}" != \
    "${short_trace}" || {
    echo "not ok" 1 - "log lines short value missed its consumer"
    exit 0
}

env_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_TRACE_TOPIC=suboption_contract,timeline_contract" \
    --env "SIXEL_LOG_LINES=3" -xhuman -~fhedt -p16 \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "log lines environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=log_lines|stored=1|binding=log_lines,log_lines_override|value=3*}" != "${env_trace}" || {
    echo "not ok" 1 - "log lines environment value was not stored"
    exit 0
}
test "${env_trace#*LSXTLN1|*line_events=1|line_stride=3*}" != \
    "${env_trace}" || {
    echo "not ok" 1 - "log lines environment value missed its consumer"
    exit 0
}

range_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_TRACE_TOPIC=suboption_contract,timeline_contract" \
    --env "SIXEL_LOG_LINES=0" -xhuman -~fhedt -p16 \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "log lines lower conversion failed"
    exit 0
}
test "${range_trace#*LSXSUB1|*key=log_lines|stored=1|binding=log_lines,log_lines_override|value=1*}" != "${range_trace}" || {
    echo "not ok" 1 - "log lines lower endpoint was not clamped"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "log lines outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "log lines image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "log lines preserves image and timeline policy"
exit 0
