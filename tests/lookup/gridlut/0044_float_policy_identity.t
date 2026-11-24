#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gridlut/float_policy_identity || {
    printf '\n%s\n' "not ok 1 - float gridlut retains its own policy identity"
    exit 0
}
printf '\n%s\n' "ok 1 - float gridlut retains its own policy identity"
exit 0
