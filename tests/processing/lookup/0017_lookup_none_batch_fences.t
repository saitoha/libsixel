#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/README.md
# Policy: docs/encoder/palette-application/dithering/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "lookup/0017_none_batch_fences" || {
    echo "not ok 1 - none batch fences"
    exit 0
}
echo "ok 1 - none batch fences"
exit 0
