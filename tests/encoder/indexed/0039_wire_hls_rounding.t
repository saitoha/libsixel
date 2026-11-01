#!/bin/sh
# DEC blue-zero HLS determines retained bytes.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0039_wire_hls_rounding" || {
    echo 'not ok 1 - wire_hls_rounding'
    exit 0
}
echo 'ok 1 - wire_hls_rounding'
exit 0
