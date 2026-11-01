#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered linear oracle mixed.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0027_ord_linear_oracle_mixed || {
    echo "not ok 1 - ordered linear oracle mixed"
    exit 0
}
echo "ok 1 - ordered linear oracle mixed"
exit 0
