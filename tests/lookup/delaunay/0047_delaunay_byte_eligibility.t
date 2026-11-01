#!/bin/sh
# Verify delaunay byte eligibility.
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/lookup_float/0001_lookup_palette byte 10 0 0 2>&1) || {
    echo "not ok 1 - delaunay byte eligibility"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/delaunay.8bit"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual byte lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - delaunay byte eligibility"
exit 0
