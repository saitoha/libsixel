#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered instance isolation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0033_ord_instance_isolation || {
    echo "not ok 1 - ordered instance isolation"
    exit 0
}
echo "ok 1 - ordered instance isolation"
exit 0
