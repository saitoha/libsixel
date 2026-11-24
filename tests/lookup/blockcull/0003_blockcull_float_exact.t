#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Bulk float blockcull lookup equals its exhaustive scan and the VP-tree metric.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/0035_blockcull_exact float || {
    printf '\n%s\n' "not ok 1 - blockcull float bulk lookup is exhaustive-exact"
    exit 0
}
printf '\n%s\n' "ok 1 - blockcull float bulk lookup is exhaustive-exact"
exit 0
