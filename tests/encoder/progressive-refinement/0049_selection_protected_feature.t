#!/bin/sh
# Verify selection protected feature.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0049_selection_protected_feature" || {
    printf '\n%s\n' "not ok 1 - selection protected feature"
    exit 0
}
printf '\n%s\n' "ok 1 - selection protected feature"
exit 0
