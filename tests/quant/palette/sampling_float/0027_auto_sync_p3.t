#!/bin/sh
# Verify auto sync p3.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    SIXEL_PALETTE_SAMPLE_TARGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/sampling_float/0001_sampling_contract \
    auto-sync 0 1 auto 2>&1) || {
    echo "not ok 1 - auto sync p3"
    printf "# %s\n" "${trace}"
    exit 0
}

case "${trace}" in *"effective=full-frame|source=preprocessed-frame"*) true ;; *) false ;; esac || {
    echo "not ok 1 - auto sync p3 dispatch"
    printf "# %s\n" "${trace}"
    exit 0
}

case "${trace}" in *"points=8|quantizer_retries=0"*) true ;; *) false ;; esac || {
    echo "not ok 1 - auto sync p3 dispatch"
    printf "# %s\n" "${trace}"
    exit 0
}

case "${trace}" in *"effective=full-frame|source=preprocessed-frame"*) true ;; *) false ;; esac || {
    echo "not ok 1 - auto sync p3 dispatch"
    printf "# %s\n" "${trace}"
    exit 0
}

echo "ok 1 - auto sync p3"
exit 0
