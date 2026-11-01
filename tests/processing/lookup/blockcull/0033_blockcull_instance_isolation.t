#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull instance isolation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0033_blockcull_instance_isolation || {
    echo "not ok 1 - blockcull instance isolation"
    exit 0
}
echo "ok 1 - blockcull instance isolation"
exit 0
