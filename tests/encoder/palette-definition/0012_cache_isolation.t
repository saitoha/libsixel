#!/bin/sh
# Native HLS cache isolation.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/hls/0012_cache_isolation || {
    echo "not ok 1 - Native HLS cache isolation."
    exit 0
}
echo "ok 1 - Native HLS cache isolation."
exit 0
