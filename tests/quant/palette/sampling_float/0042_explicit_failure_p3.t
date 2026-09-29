#!/bin/sh
# Verify explicit failure p3.
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
    _SIXEL_TEST_PALETTE_JOB_FAILURE=worker \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/sampling_float/0001_sampling_contract \
    error 0 1 auto 2>&1) || {
    echo "not ok 1 - explicit failure p3"
    printf "# %s\n" "${trace}"
    exit 0
}

case "${trace}" in *"action=propagate-explicit"*) true ;; *) false ;; esac || {
    echo "not ok 1 - explicit failure p3 dispatch"
    printf "# %s\n" "${trace}"
    exit 0
}

echo "ok 1 - explicit failure p3"
exit 0
