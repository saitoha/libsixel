#!/bin/sh
# Hard owns the same source-containing snapshot.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/clamp/0044_snapshot_allocation 1 || {
    printf '\n%s\n' "not ok 1 - hard_snapshot"
    exit 0
}
printf '\n%s\n' "ok 1 - hard_snapshot"
exit 0
