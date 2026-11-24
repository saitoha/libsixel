#!/bin/sh
# Verify a encoder p3 at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/positional/precision/0040_a_encoder_p3 2>&1) || {
    printf '\n%s\n' "not ok 1 - a_encoder_p3"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/none.float32"*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - actual float32 lookup"
    printf '# %s\n' "${trace}"
    exit 0
}

printf '\n%s\n' "ok 1 - a_encoder_p3"
exit 0
