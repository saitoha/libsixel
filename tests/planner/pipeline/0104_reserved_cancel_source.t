#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Cancellation completes every admitted source after caller references are released.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "planner/pipeline/0104_reserved_cancel_source" || {
    echo "not ok 1 - reserved cancel source"
    exit 0
}
echo "ok 1 - reserved cancel source"
exit 0
