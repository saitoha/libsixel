#!/bin/sh
# Verify automatic palette selection: explicit gamma.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/auto_float/0002_auto_explicit_gamma 2>&1) || {
    echo "not ok 1 - auto explicit gamma"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - auto explicit gamma"
exit 0
