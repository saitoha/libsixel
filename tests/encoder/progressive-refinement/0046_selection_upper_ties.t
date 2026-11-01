#!/bin/sh
# Verify selection upper ties.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0046_selection_upper_ties" || {
    echo "not ok 1 - selection upper ties"
    exit 0
}
echo "ok 1 - selection upper ties"
exit 0
