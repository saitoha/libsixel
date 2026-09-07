#!/bin/sh

# Policy: docs/loader/background-policy.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0051_loader_osc11_response_parser" || {
    echo "not ok 1 - OSC 11 response accepted framing"
    exit 0
}

echo "ok 1 - OSC 11 response accepted framing"
exit 0
