#!/bin/sh
# Verify x encoder linear at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/positional/precision/0044_x_encoder_linear 2>&1) || {
    echo "not ok 1 - x_encoder_linear"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/none.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual float32 lookup"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - x_encoder_linear"
exit 0
