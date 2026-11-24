#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered linear oracle small.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0022_ord_linear_oracle_small || {
    printf '\n%s\n' "not ok 1 - ordered linear oracle small"
    exit 0
}
printf '\n%s\n' "ok 1 - ordered linear oracle small"
exit 0
