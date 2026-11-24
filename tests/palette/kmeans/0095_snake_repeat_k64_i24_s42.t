#!/bin/sh
# Verify snake repeat k64 i24 s42.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/kmeans/0095_snake_repeat_k64_i24_s42" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - snake repeat k64 i24 s42"
    exit 0
}
printf '\n%s\n' "ok 1 - snake repeat k64 i24 s42"
exit 0
