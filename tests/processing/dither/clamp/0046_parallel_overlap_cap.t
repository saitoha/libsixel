#!/bin/sh
# Verify rounded band height and capped overlap against independent index fixtures.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP internal threads unavailable"
    set -v
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/clamp/0043_parallel_overlap 3 2>&1) || {
    echo "not ok 1 - rounded band height and capped overlap"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"clamp: compiler disables parallel apply"*)
    echo "ok 1 # SKIP compiler disables parallel palette application"
    exit 0 ;;
esac

case "${trace}" in *"selected=lookup/none.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - native float lookup was not reached"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - rounded band height and capped overlap"
exit 0
