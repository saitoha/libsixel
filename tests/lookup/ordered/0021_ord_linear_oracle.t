#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered linear oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0012_ord_linear_oracle || {
    echo "not ok 1 - ordered linear oracle"
    exit 0
}
echo "ok 1 - ordered linear oracle"
exit 0
