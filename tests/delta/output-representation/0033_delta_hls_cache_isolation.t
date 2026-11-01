#!/bin/sh
# HLS derived palette cache isolation preserves publication and ownership boundaries.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0165_delta_hls_cache_isolation || {
    echo "not ok 1 - delta hls cache isolation"
    exit 0
}
echo "ok 1 - delta hls cache isolation"
exit 0
