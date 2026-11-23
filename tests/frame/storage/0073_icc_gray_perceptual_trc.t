#!/bin/sh
set -eux
echo '1..1'
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical gray-perceptual-matrix || status=$?
test "$status" -ne 1 || {
    echo 'not ok 1 - Gray ICC perceptual LUT preserves relative kTRC # TODO parser skips kTRC when A2B0 is present'
    exit 0
}
test "$status" -ne 0 || {
    echo 'not ok 1 - Relative kTRC now works with A2B0; remove the expected failure'
    exit 0
}
echo 'not ok 1 - Gray ICC fixture or relative RGB result is incorrect'
exit 0
