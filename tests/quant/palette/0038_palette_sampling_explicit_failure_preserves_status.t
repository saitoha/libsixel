#!/bin/sh
# Verify explicit sampling propagates worker failure without changing policy.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

set +e
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_JOB_FAILURE=init \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    -Qauto:sampling_policy=adaptive-grid \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >/dev/null)
status=$?
set -e

test "${status}" -ne 0 || {
    echo "not ok 1 - explicit sampling ignored worker failure"
    exit 0
}

test "${trace#*LSXPFB1|stage=init|action=propagate-explicit|cause=4352|rc=4352*}" \
    != "${trace}" || {
    echo "not ok 1 - explicit sampling replaced the worker failure status"
    exit 0
}

test "${trace#*LSXSPL1|requested=adaptive-grid|effective=adaptive-grid|source=loaded-frame|origin=explicit|phase=resolved|reason=explicit|threads=4|heavy=0|budget_async=1|job_ready=1*}" \
    != "${trace}" || {
    echo "not ok 1 - explicit sampling failure changed policy state"
    exit 0
}

echo "ok 1 - explicit sampling preserves worker failure status"
exit 0
