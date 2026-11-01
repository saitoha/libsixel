#!/bin/sh
# Native HLS parallel body.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    set -v
    exit 0
}

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/hls/0009_parallel_body || {
    echo "not ok 1 - Native HLS parallel body."
    exit 0
}
echo "ok 1 - Native HLS parallel body."
exit 0
