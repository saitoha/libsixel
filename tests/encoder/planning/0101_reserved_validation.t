#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Reservation copies input, validates budgets and rejects conflicting changes atomically.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "planner/pipeline/0101_reserved_validation" || {
    echo "not ok 1 - reserved validation"
    exit 0
}
echo "ok 1 - reserved validation"
exit 0
