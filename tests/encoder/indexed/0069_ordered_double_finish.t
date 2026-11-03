#!/bin/sh
# Policy: docs/encoder/execution-map/README.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" component/ordered_double_finish || {
    echo "not ok 1 - ordered double finish"
    exit 0
}
echo "ok 1 - ordered double finish"
exit 0
