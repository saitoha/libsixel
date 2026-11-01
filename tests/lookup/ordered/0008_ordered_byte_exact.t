#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Bulk byte ordered lookup equals an independent exhaustive scan.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/0039_ordered_exact byte || {
    echo "not ok 1 - ordered byte bulk lookup is exhaustive-exact"
    exit 0
}
echo "ok 1 - ordered byte bulk lookup is exhaustive-exact"
exit 0
