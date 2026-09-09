#!/bin/sh
# Verify that palette-worker initialization failure keeps encoding available.

set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

output="${ARTIFACT_LOCAL_DIR}/palette-worker-init-$$.six"
message=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_JOB_FAILURE=init \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >"${output}") || {
    echo "not ok 1 - palette worker init failure stopped encoding"
    exit 0
}

test -s "${output}" || {
    echo "not ok 1 - palette worker init fallback produced no output"
    exit 0
}

test "${message#*LSXPFB1\|stage=init\|action=full-frame-sync\|*\|rc=0*}" \
    != "${message}" || {
    echo "not ok 1 - palette worker init fallback was not traced"
    exit 0
}

test "${message#*LSXSPL1\|requested=auto\|effective=full-frame\|source=preprocessed-frame\|origin=auto\|phase=executed\|reason=fallback\|threads=4\|heavy=0\|budget_async=1\|job_ready=1*}" \
    != "${message}" || {
    echo "not ok 1 - palette worker init fallback state was not recorded"
    exit 0
}

echo "ok 1 - palette worker init failure falls back to full frame"
exit 0
