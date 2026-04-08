#!/bin/sh
# TAP runner for builtin ICC B2A0/B2A1/B2A2 parser/apply coverage.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "icc/0004_icc_builtin_b2a_slot_paths" 1>&2 || {
    echo "not ok" 1 - "icc builtin B2A slot coverage"
    exit 0
}

echo "ok" 1 - "icc builtin B2A slot coverage"

exit 0
