#!/bin/sh
# Selecting delta after a prefixed PNG path preserves the configured path and history.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0012_delta_prefix_then_delta || {
    echo "not ok 1 - PNG delta prefix then delta"
    exit 0
}
echo "ok 1 - PNG delta prefix then delta"
exit 0
