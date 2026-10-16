#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify lookup failure propagation and diagnostics with and without workers.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_JOB_FAILURE=lookup \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0082_palette_lookup_failure \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || {
    printf '%s\n' "${trace}"
    echo "not ok 1 - lookup status or diagnostic changed across palette paths"
    exit 0
}

case "${trace}" in *LSXPFB1\|stage=lookup\|action=propagate\|cause=4354\|rc=4354*) true ;; *) false ;; esac || {
    echo "not ok 1 - worker lookup failure was not propagated"
    exit 0
}
case "${trace}" in *action=full-frame-sync*|*reason=fallback*) false ;; *) true ;; esac || {
    echo "not ok 1 - lookup failure retried palette construction"
    exit 0
}
echo "ok 1 - lookup failures preserve status and diagnostics without retry"
exit 0
