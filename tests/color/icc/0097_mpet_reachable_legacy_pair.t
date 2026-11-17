#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0097_mpet_reachable_legacy_pair || {
    echo "not ok 1 - reachable mpet preserves a usable legacy slot pair"
    exit 0
}

echo "ok 1 - reachable mpet preserves a usable legacy slot pair"
exit 0
