#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut instance isolation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0033_gridlut_instance_isolation || {
    printf '\n%s\n' "not ok 1 - gridlut instance isolation"
    exit 0
}
printf '\n%s\n' "ok 1 - gridlut instance isolation"
exit 0
