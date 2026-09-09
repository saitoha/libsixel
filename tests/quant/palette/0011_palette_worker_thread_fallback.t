#!/bin/sh
# Verify that thread creation failure reuses the completed sample.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

expected="${ARTIFACT_LOCAL_DIR}/palette-worker-async-$$.six"
actual="${ARTIFACT_LOCAL_DIR}/palette-worker-thread-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" >"${expected}" || {
    echo "not ok 1 - reference palette worker failed"
    exit 0
}

message=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_JOB_FAILURE=thread \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >"${actual}") || {
    echo "not ok 1 - palette thread failure stopped encoding"
    exit 0
}

cmp -s "${expected}" "${actual}" || {
    echo "not ok 1 - palette thread fallback changed the sample result"
    exit 0
}

test "${message#*LSXPFB1\|stage=thread-create\|action=same-sample-sync\|*\|rc=0*}" \
    != "${message}" || {
    echo "not ok 1 - palette thread fallback was not traced"
    exit 0
}

test "${message#*LSXSPL1\|requested=auto\|effective=adaptive-grid\|source=loaded-frame\|origin=auto\|phase=executed\|reason=resource-profile\|threads=4\|heavy=0\|budget_async=1\|job_ready=1*}" \
    != "${message}" || {
    echo "not ok 1 - palette thread fallback changed sampling state"
    exit 0
}

echo "ok 1 - palette thread failure reuses the completed sample"
exit 0
