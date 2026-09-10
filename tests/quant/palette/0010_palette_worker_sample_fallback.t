#!/bin/sh
# Verify that sample creation failure uses the full-frame fallback.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

expected="${ARTIFACT_LOCAL_DIR}/palette-worker-init-$$.six"
actual="${ARTIFACT_LOCAL_DIR}/palette-worker-sample-$$.six"

_SIXEL_TEST_PALETTE_JOB_FAILURE=init \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" >"${expected}" || {
    echo "not ok 1 - reference full-frame fallback failed"
    exit 0
}

message=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_JOB_FAILURE=sample \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" 2>&1 >"${actual}") || {
    echo "not ok 1 - palette sample failure stopped encoding"
    exit 0
}

cmp -s "${expected}" "${actual}" || {
    echo "not ok 1 - palette sample failure did not use full frame"
    exit 0
}

test "${message#*LSXPFB1\|stage=sample\|action=full-frame-sync\|*\|rc=0*}" \
    != "${message}" || {
    echo "not ok 1 - palette sample fallback was not traced"
    exit 0
}

test "${message#*LSXSPL1\|requested=auto\|effective=full-frame\|source=preprocessed-frame\|origin=auto\|phase=executed\|reason=fallback\|threads=4\|heavy=0\|budget_async=1\|job_ready=1*}" \
    != "${message}" || {
    echo "not ok 1 - palette sample fallback state was not recorded"
    exit 0
}

echo "ok 1 - palette sample failure falls back to full frame"
exit 0
