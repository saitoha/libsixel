#!/bin/sh
# Verify automatic palette selection: byte hard kmeans.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/auto_float/0019_auto_byte_hard_kmeans 2>&1) || {
    printf '\n%s\n' "not ok 1 - auto byte hard kmeans"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.8bit"*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - actual lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

printf '\n%s\n' "ok 1 - auto byte hard kmeans"
exit 0
