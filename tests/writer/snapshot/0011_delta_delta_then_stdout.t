#!/bin/sh
# Selecting a PNG stdout target after delta rejects before changing output state.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0011_delta_delta_then_stdout || {
    echo "not ok 1 - PNG delta delta then stdout"
    exit 0
}
echo "ok 1 - PNG delta delta then stdout"
exit 0
