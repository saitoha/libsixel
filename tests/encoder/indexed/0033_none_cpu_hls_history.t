#!/bin/sh
# none CPU HLS preserves legacy retained pixels and validity.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0033_none_cpu_hls_history" || {
    echo 'not ok 1 - 0033_none_cpu_hls_history'
    exit 0
}
echo 'ok 1 - 0033_none_cpu_hls_history'
exit 0
