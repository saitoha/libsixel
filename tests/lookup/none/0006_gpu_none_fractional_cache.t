#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/none.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/none_fractional_cache || {
    echo "not ok 1 - none fractional cache"
    exit 0
}
echo "ok 1 - none fractional cache"
exit 0
