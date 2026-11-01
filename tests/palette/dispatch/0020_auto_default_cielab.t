#!/bin/sh
# Verify automatic palette selection: default cielab.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/auto_float/0006_auto_default_cielab 2>&1) || {
    echo "not ok 1 - auto default cielab"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - auto default cielab"
exit 0
