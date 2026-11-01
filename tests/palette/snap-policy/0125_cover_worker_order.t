#!/bin/sh
# Verify native snap and cover: cover worker order.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract cover 0 0 1 5 2>&1) || {
    echo "not ok 1 - cover worker order"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"budget_async=1|job_ready=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - native snap and cover worker dispatch"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - cover worker order"
exit 0
