#!/bin/sh
# Verify byte fast path.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    SIXEL_PALETTE_SAMPLE_TARGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/sampling_float/0001_sampling_contract \
    byte 0 0 auto 2>&1) || {
    echo "not ok 1 - byte fast path"
    printf "# %s\n" "${trace}"
    exit 0
}

case "${trace}" in *"points=2|quantizer_retries=0"*) true ;; *) false ;; esac || {
    echo "not ok 1 - byte fast path dispatch"
    printf "# %s\n" "${trace}"
    exit 0
}

echo "ok 1 - byte fast path"
exit 0
