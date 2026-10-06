#!/bin/sh
# Selecting delta after a PNG stdout target preserves destination and history on rejection.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0010_delta_stdout_then_delta || {
    echo "not ok 1 - PNG delta stdout then delta"
    exit 0
}
echo "ok 1 - PNG delta stdout then delta"
exit 0
