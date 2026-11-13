#!/bin/sh
# Verify reject lso2 method.
# Policy: docs/encoder/palette-application/dithering/stucki-gpu.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/stucki-gpu/0004_reject_lso2_method" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - reject lso2 method"
    exit 0
}
echo "ok 1 - reject lso2 method"
exit 0
