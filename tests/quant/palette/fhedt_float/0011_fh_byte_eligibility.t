#!/bin/sh
# Verify fhedt byte eligibility.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/fhedt_float/0011_fh_byte_eligibility 2>&1) || {
    echo "not ok 1 - fhedt byte eligibility"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/fhedt.8bit"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual byte lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - fhedt byte eligibility"
exit 0
