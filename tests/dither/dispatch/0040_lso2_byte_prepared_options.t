#!/bin/sh
# Verify one prepared policy and representation.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/dispatch/0040_lso2_byte_prepared_options" || {
    printf '\n%s\n' 'not ok 1 - 0040_lso2_byte_prepared_options'
    exit 0
}
printf '\n%s\n' 'ok 1 - 0040_lso2_byte_prepared_options'
exit 0
