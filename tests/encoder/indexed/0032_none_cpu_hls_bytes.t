#!/bin/sh
# none CPU HLS preserves legacy stream bytes.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0032_none_cpu_hls_bytes" || {
    echo 'not ok 1 - 0032_none_cpu_hls_bytes'
    exit 0
}
echo 'ok 1 - 0032_none_cpu_hls_bytes'
exit 0
