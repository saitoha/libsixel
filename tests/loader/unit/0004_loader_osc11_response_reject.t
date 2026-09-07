#!/bin/sh

# Policy: docs/loader/background-policy.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0066_loader_osc11_response_reject" || {
    echo "not ok 1 - OSC 11 response rejected framing"
    exit 0
}

echo "ok 1 - OSC 11 response rejected framing"
exit 0
