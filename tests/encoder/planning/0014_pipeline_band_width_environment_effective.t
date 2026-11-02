#!/bin/sh
# Verify the effective pipeline band height selected by the environment.

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
expected_band=18
test "$parallel_dither" = 1 || expected_band=66

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

width_trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_ENCODER_THREADING_POLICY=greedy" --env "SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=6" \
    --env "SIXEL_DITHER_PARALLEL_BAND_WIDTH=9" -d fs \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "band width environment conversion failed"
    exit 0
}

case "${width_trace}" in *LSXDTH1\|*diffuse=fs\|*band_height=12**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "band width was not rounded to six rows"
    exit 0
}

negative_trace=$(set +xv; SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_ENCODER_THREADING_POLICY=greedy" --env "SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=6" \
    --env "SIXEL_DITHER_PARALLEL_BAND_WIDTH=-1" -d fs \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "negative band width conversion failed"
    exit 0
}

case "${negative_trace}" in *LSXDTH1\|*diffuse=fs\|*band_height=${expected_band}**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "negative band width changed the default"
    exit 0
}

echo "ok" 1 - "band width environment controls effective geometry"
exit 0
