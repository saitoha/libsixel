#!/bin/sh

# Policy: docs/loader/alpha-policy.md
# Policy: docs/loader/background-policy.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0052_loader_osc11_query_control" || {
    echo "not ok 1 - OSC 11 loader query gate"
    exit 0
}

echo "ok 1 - OSC 11 loader query gate"
exit 0
