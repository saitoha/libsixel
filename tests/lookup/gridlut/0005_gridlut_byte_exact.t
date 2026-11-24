#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Bulk byte gridlut lookup equals an independent exhaustive scan.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/0037_gridlut_exact byte || {
    printf '\n%s\n' "not ok 1 - gridlut byte bulk lookup is exhaustive-exact"
    exit 0
}
printf '\n%s\n' "ok 1 - gridlut byte bulk lookup is exhaustive-exact"
exit 0
