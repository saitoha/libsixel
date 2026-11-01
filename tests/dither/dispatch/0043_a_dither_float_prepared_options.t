#!/bin/sh
# Verify one prepared policy and representation.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/dispatch/0043_a_dither_float_prepared_options" || {
    echo 'not ok 1 - 0043_a_dither_float_prepared_options'
    exit 0
}
echo 'ok 1 - 0043_a_dither_float_prepared_options'
exit 0
