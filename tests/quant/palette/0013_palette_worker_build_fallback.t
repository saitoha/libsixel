#!/bin/sh
# Verify that a failed sampled palette build retries the full frame.

set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

expected="${ARTIFACT_LOCAL_DIR}/palette-worker-init-$$.six"
actual="${ARTIFACT_LOCAL_DIR}/palette-worker-build-$$.six"

_SIXEL_TEST_PALETTE_JOB_FAILURE=init \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" >"${expected}" || {
    echo "not ok 1 - reference full-frame fallback failed"
    exit 0
}

message=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_JOB_FAILURE=worker \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >"${actual}") || {
    echo "not ok 1 - sampled palette failure stopped encoding"
    exit 0
}

cmp -s "${expected}" "${actual}" || {
    echo "not ok 1 - sampled palette failure did not use full frame"
    exit 0
}

test "${message#*LSXPFB1\|stage=worker-build\|action=full-frame-sync\|*\|rc=0*}" \
    != "${message}" || {
    echo "not ok 1 - sampled palette fallback was not traced"
    exit 0
}

test "${message#*LSXSPL1\|requested=auto\|effective=full-frame\|source=preprocessed-frame\|origin=auto\|phase=executed\|reason=fallback\|threads=4\|heavy=0\|budget_async=1\|job_ready=1*}" \
    != "${message}" || {
    echo "not ok 1 - sampled palette fallback state was not recorded"
    exit 0
}

echo "ok 1 - sampled palette failure falls back to full frame"
exit 0
