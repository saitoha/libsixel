#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0053_status_mpet_slot3_fallback || {
    echo "not ok 1 - profile open reports skipped absolute mpet slots"
    exit 0
}

echo "ok 1 - profile open reports skipped absolute mpet slots"
exit 0
