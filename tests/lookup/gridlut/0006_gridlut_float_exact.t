#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Bulk float gridlut lookup equals its exhaustive scan and the VP-tree metric.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/0037_gridlut_exact float || {
    echo "not ok 1 - gridlut float bulk lookup is exhaustive-exact"
    exit 0
}
echo "ok 1 - gridlut float bulk lookup is exhaustive-exact"
exit 0
