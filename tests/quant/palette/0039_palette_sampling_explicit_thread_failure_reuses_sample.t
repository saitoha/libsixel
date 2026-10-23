#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify explicit adaptive sampling reuses its sample after thread failure.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# Keep this scheduling contract free of loader color-conversion work.
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_JOB_FAILURE=thread \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=4 \
    --sampling-policy=adaptive-grid \
    -Qheckbert -d none -p 16 "-~none" --cms-engine=none -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - explicit sampling thread fallback stopped encoding"
    exit 0
}

case "${trace}" in *LSXPFB1\|stage=thread-create\|action=same-sample-sync\|*\|rc=0**) true ;; *) false ;; esac || {
    echo "not ok 1 - explicit sampling did not reuse the completed sample"
    exit 0
}

case "${trace}" in *LSXSPL1\|requested=adaptive-grid\|effective=adaptive-grid\|source=loaded-frame\|origin=explicit\|phase=executed\|reason=explicit\|threads=4\|heavy=0\|budget_async=1\|job_ready=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - explicit thread fallback changed sampling state"
    exit 0
}

echo "ok 1 - explicit adaptive sampling reuses its completed sample"
exit 0
