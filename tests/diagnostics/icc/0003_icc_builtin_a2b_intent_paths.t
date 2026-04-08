#!/bin/sh
# TAP runner for builtin ICC A2B0/A2B1/A2B2 intent path coverage.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "icc/0003_icc_builtin_a2b_intent_paths" 1>&2 || {
    echo "not ok" 1 - "icc builtin A2B intent coverage"
    exit 0
}

echo "ok" 1 - "icc builtin A2B intent coverage"

exit 0
