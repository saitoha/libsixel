#!/bin/sh
# Verify rare repeat k64 i12 s7.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/kmeans/0103_rare_repeat_k64_i12_s7" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - rare repeat k64 i12 s7"
    exit 0
}
printf '\n%s\n' "ok 1 - rare repeat k64 i12 s7"
exit 0
