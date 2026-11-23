#!/bin/sh
# Future opt-in target policy: train -X on continuous rendered samples.
# Do not snap those samples or render a fitted center through the LUT twice.
# Bind the selector here when implementing the public option.
set -eux
echo '1..1'
set -v

status=0
SIXEL_PALETTE_TARGET=output SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/icc_output/0001_icc_output output-target cluster \
    "${TOP_SRCDIR}/tests/data/color-numeric" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP required backend unavailable'
    exit 0
}
test "$status" -ne 42 || {
    echo 'not ok 1 - clustering still fits extended input coordinates # TODO known result'
    exit 0
}
test "$status" -ne 0 || {
    echo 'not ok 1 - expected behavior now works; promote this test'
    exit 0
}
echo 'not ok 1 - unexpected fixture, execution or pixel result'
exit 0
