#!/bin/sh
# Low-level legacy byte diffusion converts output coordinates before its byte boundary.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0013_lowlevel_diffusion || {
    echo "not ok 1 - high-color lowlevel diffusion"
    exit 0
}
echo "ok 1 - high-color lowlevel diffusion"
exit 0
