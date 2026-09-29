#!/bin/sh
# Verify kcenter worker p3.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    SIXEL_PALETTE_SAMPLE_TARGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/solvers_float/0001_solver_contract \
    worker 0 1 2 1 2>&1) || {
    echo "not ok 1 - kcenter worker p3"
    printf "# %s\n" "${trace}"
    exit 0
}

case "${trace}" in *"job_ready=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - kcenter worker p3 dispatch"
    printf "# %s\n" "${trace}"
    exit 0
}

echo "ok 1 - kcenter worker p3"
exit 0
