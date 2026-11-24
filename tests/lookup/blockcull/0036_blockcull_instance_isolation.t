#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull instance isolation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0033_blockcull_instance_isolation || {
    printf '\n%s\n' "not ok 1 - blockcull instance isolation"
    exit 0
}
printf '\n%s\n' "ok 1 - blockcull instance isolation"
exit 0
