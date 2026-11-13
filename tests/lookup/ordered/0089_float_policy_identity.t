#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" ordered/float_policy_identity || {
    echo "not ok 1 - float ordered retains its own policy identity"
    exit 0
}
echo "ok 1 - float ordered retains its own policy identity"
exit 0
