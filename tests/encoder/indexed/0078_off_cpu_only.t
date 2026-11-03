#!/bin/sh
# Policy: docs/encoder/execution-map/README.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" component/off_cpu_only || {
    echo "not ok 1 - off cpu only"
    exit 0
}
echo "ok 1 - off cpu only"
exit 0
