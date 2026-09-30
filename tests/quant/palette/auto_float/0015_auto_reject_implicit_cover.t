#!/bin/sh
# Verify automatic palette selection: reject implicit cover.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/auto_float/0015_auto_reject_implicit_cover 2>&1) || {
    echo "not ok 1 - auto reject implicit cover"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - auto reject implicit cover"
exit 0
