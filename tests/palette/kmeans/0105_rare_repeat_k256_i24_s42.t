#!/bin/sh
# Verify rare repeat k256 i24 s42.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/kmeans/0105_rare_repeat_k256_i24_s42" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - rare repeat k256 i24 s42"
    exit 0
}
echo "ok 1 - rare repeat k256 i24 s42"
exit 0
