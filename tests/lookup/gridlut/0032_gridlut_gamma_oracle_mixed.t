#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut gamma oracle mixed.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0026_gridlut_gamma_oracle_mixed || {
    printf '\n%s\n' "not ok 1 - gridlut gamma oracle mixed"
    exit 0
}
printf '\n%s\n' "ok 1 - gridlut gamma oracle mixed"
exit 0
