#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull oklab fraction.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0008_blockcull_oklab_fraction || {
    echo "not ok 1 - blockcull oklab fraction"
    exit 0
}
echo "ok 1 - blockcull oklab fraction"
exit 0
