#!/bin/sh
# RGB and HLS syntax changes preserve compatible history and compare their distinct emitted colors.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0172_delta_hls_syntax_reuse || {
    echo "not ok 1 - delta hls syntax reuse"
    exit 0
}
echo "ok 1 - delta hls syntax reuse"
exit 0
