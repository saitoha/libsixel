#!/bin/sh
# Verify or float mask recovery with independent values and indices.
# Policy: docs/encoder/output/or-mode/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder-core/0038_or_float_mask_recovery || {
    echo "not ok 1 - or float mask recovery"
    exit 0
}
echo "ok 1 - or float mask recovery"
exit 0
