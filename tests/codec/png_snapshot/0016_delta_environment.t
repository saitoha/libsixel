#!/bin/sh
# Environment-selected delta rejects PNG snapshot selection.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_UPDATE_POLICY=delta ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0016_delta_environment || {
    echo "not ok 1 - PNG delta environment"
    exit 0
}
echo "ok 1 - PNG delta environment"
exit 0
