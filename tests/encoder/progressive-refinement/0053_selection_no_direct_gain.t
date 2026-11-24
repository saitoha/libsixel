#!/bin/sh
# Verify selection no direct gain.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0053_selection_no_direct_gain" || {
    printf '\n%s\n' "not ok 1 - selection no direct gain"
    exit 0
}
printf '\n%s\n' "ok 1 - selection no direct gain"
exit 0
