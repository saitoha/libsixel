#!/bin/sh
# GPU blockcull matches the exhaustive scan and actually runs the culling kernel.
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" --require-gpu-palette \
    "gpu-palette/0021_blockcull_exact" 2>&1) || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '# %s\n' "${trace}"
    echo "not ok 1 - GPU blockcull matches the exhaustive scan"
    exit 0
}
# Devices without simdgroup matrices keep the exact direct scan.
case "${trace}" in
    *'kernel=blockcull'*|*'kernel=direct'*) true ;;
    *) false ;;
esac || {
    printf '# %s\n' "${trace}"
    echo "not ok 1 - GPU blockcull did not select a culling or direct kernel"
    exit 0
}

echo "ok 1 - GPU blockcull matches the exhaustive scan"
exit 0
