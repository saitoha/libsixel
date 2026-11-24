#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut cielab range.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0004_gridlut_cielab_range || {
    printf '\n%s\n' "not ok 1 - gridlut cielab range"
    exit 0
}
printf '\n%s\n' "ok 1 - gridlut cielab range"
exit 0
