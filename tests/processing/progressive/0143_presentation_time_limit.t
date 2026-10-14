#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify presentation time limit.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0143_presentation_time_limit" || {
    echo "not ok 1 - presentation time limit"
    exit 0
}
echo "ok 1 - presentation time limit"
exit 0
