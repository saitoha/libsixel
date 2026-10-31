#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Bulk float ordered lookup equals its exhaustive scan and the VP-tree metric.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/0039_ordered_exact float || {
    echo "not ok 1 - ordered float bulk lookup is exhaustive-exact"
    exit 0
}
echo "ok 1 - ordered float bulk lookup is exhaustive-exact"
exit 0
