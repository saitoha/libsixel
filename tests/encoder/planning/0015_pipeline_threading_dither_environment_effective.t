#!/bin/sh
# Verify the effective pipeline thread split selected by the environment.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP threading is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# Read the compiled dither capability; keep serial option routing covered.
parallel_dither=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/allocation/0001_allocation_slots parallel-dither) || {
    echo "not ok 1 - cannot query the compiled dither capability"
    exit 0
}
expected_dither=4
test "$parallel_dither" = 1 || expected_dither=1

# Automatic sampling releases its palette slots before this pipeline.
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

threads_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_ENCODER_THREADING_POLICY=greedy" --env "SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=6" \
    --env "SIXEL_ENCODER_THREADING_DITHER=1" -v -d fs \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "threading dither environment conversion failed"
    exit 0
}

case "${threads_trace}" in *threads:\ dither=1\ encode=5**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading dither did not cap the dither workers"
    exit 0
}

negative_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_ENCODER_THREADING_POLICY=greedy" --env "SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=6" \
    --env "SIXEL_ENCODER_THREADING_DITHER=-1" -v -d fs \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "negative threading dither conversion failed"
    exit 0
}

case "${negative_trace}" in *threads:\ dither=${expected_dither}\ encode=2**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "negative threading dither changed the default"
    exit 0
}

echo "ok" 1 - "threading dither environment controls the worker split"
exit 0
