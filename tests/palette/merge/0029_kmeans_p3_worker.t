#!/bin/sh
# Verify kmeans p3 worker.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract worker 0 1 1 2>&1) || {
    echo "not ok 1 - kmeans p3 worker"
    exit 0
}

case "${trace}" in *"job_ready=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - Ward palette worker dispatch"
    printf "# %s\n" "${trace}"
    exit 0
}

echo "ok 1 - kmeans p3 worker"
exit 0
