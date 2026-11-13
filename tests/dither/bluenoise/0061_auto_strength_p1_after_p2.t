#!/bin/sh
# Verify auto strength p1 after p2.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup none \
    "dither/bluenoise/0061_auto_strength_p1_after_p2" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - auto strength p1 after p2"
    exit 0
}
echo "ok 1 - auto strength p1 after p2"
exit 0
