#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify still capture history.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0101_still_capture_history" || {
    echo "not ok 1 - still capture history"
    exit 0
}
echo "ok 1 - still capture history"
exit 0
