#!/bin/sh
# Verify a encoder byte at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/positional/precision/0041_a_encoder_byte 2>&1) || {
    echo "not ok 1 - a_encoder_byte"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/none.8bit"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual 8bit lookup"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - a_encoder_byte"
exit 0
