#!/bin/sh
# Verify automatic palette selection: default din99d.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/auto_float/0007_auto_default_din99d 2>&1) || {
    printf '\n%s\n' "not ok 1 - auto default din99d"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.float32"*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - actual lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

printf '\n%s\n' "ok 1 - auto default din99d"
exit 0
