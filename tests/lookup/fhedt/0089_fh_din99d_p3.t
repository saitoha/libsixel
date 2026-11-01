#!/bin/sh
# Verify fhedt din99d p3.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/fhedt_float/0010_fh_din99d_p3 2>&1) || {
    echo "not ok 1 - fhedt din99d p3"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/fhedt.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual float lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - fhedt din99d p3"
exit 0
