#!/bin/sh
# Verify ladder resumable identity.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0133_ladder_resumable_identity" || {
    printf '\n%s\n' "not ok 1 - ladder resumable identity"
    exit 0
}
printf '\n%s\n' "ok 1 - ladder resumable identity"
exit 0
