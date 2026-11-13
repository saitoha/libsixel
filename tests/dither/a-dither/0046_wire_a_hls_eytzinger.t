#!/bin/sh
# Verify wire a hls eytzinger.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup eytzinger \
    "dither/a-dither/0046_wire_a_hls_eytzinger" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - wire a hls eytzinger"
    exit 0
}
echo "ok 1 - wire a hls eytzinger"
exit 0
