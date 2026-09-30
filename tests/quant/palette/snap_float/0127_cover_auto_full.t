#!/bin/sh
# Verify native snap and cover: cover auto full.
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract cover 4 0 1 1 2>&1) || {
    echo "not ok 1 - cover auto full"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - cover auto full"
exit 0
