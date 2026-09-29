#!/bin/sh
# Verify eytzinger byte eligibility.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/eytzinger_float/0011_eyt_byte_eligibility 2>&1) || {
    echo "not ok 1 - eytzinger byte eligibility"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/eytzinger.8bit"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual byte lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - eytzinger byte eligibility"
exit 0
