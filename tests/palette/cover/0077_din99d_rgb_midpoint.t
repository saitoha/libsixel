#!/bin/sh
# Verify native soft cover: din99d RGB midpoint.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cover_float/0001_cover_contract midpoint 0 4 0 || {
    printf '\n%s\n' "not ok 1 - din99d RGB midpoint"
    exit 0
}

printf '\n%s\n' "ok 1 - din99d RGB midpoint"
exit 0
