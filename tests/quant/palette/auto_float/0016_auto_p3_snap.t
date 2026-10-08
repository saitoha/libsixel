#!/bin/sh
# Verify automatic palette selection: P3 snap.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/auto_float/0016_auto_p3_snap 2>&1) || {
    echo "not ok 1 - auto P3 snap"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - auto P3 snap"
exit 0
