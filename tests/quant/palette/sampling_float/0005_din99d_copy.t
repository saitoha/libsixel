#!/bin/sh
# Verify din99d copy.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    SIXEL_PALETTE_SAMPLE_TARGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/sampling_float/0001_sampling_contract \
    copy 4 0 auto 2>&1) || {
    echo "not ok 1 - din99d copy"
    printf "# %s\n" "${trace}"
    exit 0
}

echo "ok 1 - din99d copy"
exit 0
