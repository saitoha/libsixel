#!/bin/sh
# Verify certlut bands shared.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/lookup_float/0001_lookup_palette bands 0 0 1 2>&1) || {
    echo "not ok 1 - certlut bands shared"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/certlut.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual float lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - certlut bands shared"
exit 0
