#!/bin/sh
# Verify reject perturbation.
# Policy: docs/encoder/palette-application/dithering/jajuni-gpu.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/jajuni-gpu/0006_reject_perturbation" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - reject perturbation"
    exit 0
}
echo "ok 1 - reject perturbation"
exit 0
