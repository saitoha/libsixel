#!/bin/sh
# Verify ladder tied thresholds.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0131_ladder_tied_thresholds" || {
    echo "not ok 1 - ladder tied thresholds"
    exit 0
}
echo "ok 1 - ladder tied thresholds"
exit 0
