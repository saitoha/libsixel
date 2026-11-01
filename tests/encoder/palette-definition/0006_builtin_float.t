#!/bin/sh
# Native HLS builtin float.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/hls/0006_builtin_float || {
    echo "not ok 1 - Native HLS builtin float."
    exit 0
}
echo "ok 1 - Native HLS builtin float."
exit 0
