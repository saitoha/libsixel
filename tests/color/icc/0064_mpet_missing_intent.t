#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0064_mpet_missing_intent || {
    echo "not ok 1 - absent mpet intent cannot produce a black fallback"
    exit 0
}

echo "ok 1 - absent mpet intent cannot produce a black fallback"
exit 0
