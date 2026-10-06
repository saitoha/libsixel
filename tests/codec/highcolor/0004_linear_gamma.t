#!/bin/sh
# Linear float input reaches gamma output with the transfer function applied.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0004_linear_gamma || {
    echo "not ok 1 - high-color linear gamma"
    exit 0
}
echo "ok 1 - high-color linear gamma"
exit 0
