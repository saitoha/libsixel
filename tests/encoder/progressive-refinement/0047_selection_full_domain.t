#!/bin/sh
# Verify selection full domain.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0047_selection_full_domain" || {
    printf '\n%s\n' "not ok 1 - selection full domain"
    exit 0
}
printf '\n%s\n' "ok 1 - selection full domain"
exit 0
