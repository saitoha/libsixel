#!/bin/sh
# Verify native hard cover: hard worker grow.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cover_float/0001_cover_contract wire 0 0 10 2>&1) || {
    echo "not ok 1 - hard worker grow"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"budget_async=1|job_ready=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - native cover worker dispatch"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - hard worker grow"
exit 0
