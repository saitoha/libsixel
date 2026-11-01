#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered linear oracle large.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0017_ord_linear_oracle_large || {
    echo "not ok 1 - ordered linear oracle large"
    exit 0
}
echo "ok 1 - ordered linear oracle large"
exit 0
