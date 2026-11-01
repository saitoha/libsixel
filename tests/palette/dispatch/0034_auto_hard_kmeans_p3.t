#!/bin/sh
# Verify automatic palette selection: hard kmeans p3.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/auto_float/0020_auto_hard_kmeans_p3 2>&1) || {
    echo "not ok 1 - auto hard kmeans p3"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - auto hard kmeans p3"
exit 0
