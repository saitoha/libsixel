#!/bin/sh
# Verify or float p3 indices with independent values and indices.
# Policy: docs/encoder/output/or-mode/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder-core/0033_or_float_p3_indices || {
    echo "not ok 1 - or float p3 indices"
    exit 0
}
echo "ok 1 - or float p3 indices"
exit 0
