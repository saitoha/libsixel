#!/bin/sh
# GPU ordered matches the exhaustive scan and actually runs the ordered kernel.
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md

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
    "gpu-palette/0023_ordered_exact" 2>&1) || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '# %s\n' "${trace}"
    echo "not ok 1 - GPU ordered matches the exhaustive scan"
    exit 0
}
# The dedicated trace proves the policy was not silently forced to scan.
case "${trace}" in
    *'kernel=ordered'*) true ;;
    *) false ;;
esac || {
    printf '# %s\n' "${trace}"
    echo "not ok 1 - GPU ordered did not select the ordered kernel"
    exit 0
}

# A fixed palette must reuse its table, while changes require a rebuild.
case "${trace}" in
    *'lookup=ordered|table=rebuild'*'lookup=ordered|table=reuse'*) true ;;
    *) false ;;
esac || {
    printf '# %s\n' "${trace}"
    echo "not ok 1 - GPU ordered table reuse was not observed"
    exit 0
}

echo "ok 1 - GPU ordered matches the exhaustive scan"
exit 0
