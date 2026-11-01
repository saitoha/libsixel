#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered oklab fraction.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0008_ord_oklab_fraction || {
    echo "not ok 1 - ordered oklab fraction"
    exit 0
}
echo "ok 1 - ordered oklab fraction"
exit 0
