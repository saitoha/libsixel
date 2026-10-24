#!/bin/sh
# Policy: docs/cli/runtime-policy.md
# Frame conversion uses the common buffer driver owning this threshold.
# Early snap also exercises a non-native palette-output caller.
# Request two workers so one-CPU hosts still consult the fan-out threshold.
# Verify the colorspace threshold environment reaches its consumer.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=runtime_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=balanced:global_budget=2 \
    --env SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS=257 \
    -_nearest:Iall -W oklab -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.png" 2>&1) || {
    echo "not ok 1 - colorspace threshold conversion failed"
    exit 0
}

case "${trace}" in *LSXRT1\|colorspace_min=257**) true ;; *) false ;; esac || {
    echo "not ok 1 - colorspace threshold missed its consumer"
    exit 0
}

echo "ok 1 - colorspace threshold reaches its consumer"
exit 0
