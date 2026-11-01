#!/bin/sh
# Wire RGB percentages determine retained bytes.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0038_wire_rgb_rounding" || {
    echo 'not ok 1 - wire_rgb_rounding'
    exit 0
}
echo 'ok 1 - wire_rgb_rounding'
exit 0
