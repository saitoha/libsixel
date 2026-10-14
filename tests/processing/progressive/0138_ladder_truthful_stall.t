#!/bin/sh
# Verify ladder truthful stall.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0138_ladder_truthful_stall" || {
    echo "not ok 1 - ladder truthful stall"
    exit 0
}
echo "ok 1 - ladder truthful stall"
exit 0
