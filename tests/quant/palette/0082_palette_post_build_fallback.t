#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify a late palette failure discards executed state before retrying.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

expected="${ARTIFACT_LOCAL_DIR}/palette-post-build-reference-$$.six"
actual="${ARTIFACT_LOCAL_DIR}/palette-post-build-actual-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=balanced:global_budget=8 \
    -4 full-frame -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" >"${expected}" || {
    echo "not ok 1 - reference full-frame fallback failed"
    exit 0
}

message=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_JOB_FAILURE=worker-post-build \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=balanced:global_budget=8 -4 auto \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" 2>&1 >"${actual}") || {
    echo "not ok 1 - post-build palette failure stopped encoding"
    exit 0
}

cmp -s "${expected}" "${actual}" || {
    echo "not ok 1 - post-build palette failure did not use full frame"
    exit 0
}

case "${message}" in *LSXPFB1\|stage=worker-post-build\|action=full-frame-sync\|*\|rc=0**) true ;; *) false ;; esac || {
    echo "not ok 1 - post-build palette fallback was not traced"
    exit 0
}

case "${message}" in *LSXSPL1\|requested=auto\|effective=full-frame\|source=preprocessed-frame\|origin=auto\|phase=executed\|reason=fallback\|threads=8\|heavy=0\|budget_async=1\|job_ready=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - post-build palette fallback state was not recorded"
    exit 0
}

echo "ok 1 - post-build palette failure falls back to full frame"
exit 0
