#!/bin/sh
# Future opt-in target policy: preserve the current default. Render through
# the destination once, and diffuse in -W against actual register colors.
# Keep the continuous pixel target unsnapped, including exact-color/skip
# decisions. When extending the policy to delta, compare against retained
# emitted colors. Share immutable storage across equivalent color spaces.
# Evaluate quality on real images alongside stream size and encoding time
# before proposing a default change.
# Bind the selector here when implementing the public option.
set -eux
echo '1..1'
set -v

status=0
SIXEL_PALETTE_TARGET=output SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/icc_output/0001_icc_output output-target fs \
    "${TOP_SRCDIR}/tests/data/color-numeric" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP required backend unavailable'
    exit 0
}
test "$status" -ne 42 || {
    echo 'not ok 1 - FS still compares the unrendered source # TODO known result'
    exit 0
}
test "$status" -ne 0 || {
    echo 'not ok 1 - expected behavior now works; promote this test'
    exit 0
}
echo 'not ok 1 - unexpected fixture, execution or pixel result'
exit 0
