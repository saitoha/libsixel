#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "lookup/0015_batch_contract" || {
    echo "not ok 1 - batch contract"
    exit 0
}
echo "ok 1 - batch contract"
exit 0
