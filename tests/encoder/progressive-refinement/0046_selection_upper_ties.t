#!/bin/sh
# Verify selection upper ties.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0046_selection_upper_ties" || {
    printf '\n%s\n' "not ok 1 - selection upper ties"
    exit 0
}
printf '\n%s\n' "ok 1 - selection upper ties"
exit 0
