#!/bin/sh
# Verify auto strength p64 after p256.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup none \
    "dither/bluenoise/0057_auto_strength_p64_after_p256" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - auto strength p64 after p256"
    exit 0
}
printf '\n%s\n' "ok 1 - auto strength p64 after p256"
exit 0
