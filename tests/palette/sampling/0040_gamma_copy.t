#!/bin/sh
# Verify gamma copy.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    SIXEL_PALETTE_SAMPLE_TARGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/sampling_float/0001_sampling_contract \
    copy 0 0 auto 2>&1) || {
    printf '\n%s\n' "not ok 1 - gamma copy"
    printf "# %s\n" "${trace}"
    exit 0
}

printf '\n%s\n' "ok 1 - gamma copy"
exit 0
