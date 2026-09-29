#!/bin/sh
# Verify 6bit gamma srgb.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/lookup_float/0001_lookup_palette wire 2 0 0 2>&1) || {
    echo "not ok 1 - 6bit gamma srgb"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/6bit.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual float lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - 6bit gamma srgb"
exit 0
