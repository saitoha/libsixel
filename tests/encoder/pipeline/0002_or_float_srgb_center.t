#!/bin/sh
# Verify or float srgb center with independent values and indices.
# Policy: docs/encoder/output/or-mode/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder-core/0030_or_float_srgb_center || {
    echo "not ok 1 - or float srgb center"
    exit 0
}
echo "ok 1 - or float srgb center"
exit 0
