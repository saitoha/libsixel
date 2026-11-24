#!/bin/sh
# Verify x encoder gamma at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/positional/precision/0043_x_encoder_gamma 2>&1) || {
    printf '\n%s\n' "not ok 1 - x_encoder_gamma"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/none.float32"*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - actual float32 lookup"
    printf '# %s\n' "${trace}"
    exit 0
}

printf '\n%s\n' "ok 1 - x_encoder_gamma"
exit 0
