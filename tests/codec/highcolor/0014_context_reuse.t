#!/bin/sh
# One high-color context supports repeated float and byte output conversions without stale format state.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0014_context_reuse || {
    echo "not ok 1 - high-color context reuse"
    exit 0
}
echo "ok 1 - high-color context reuse"
exit 0
