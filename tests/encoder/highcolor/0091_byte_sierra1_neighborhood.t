#!/bin/sh
# Check decoded byte samples for high-color Sierra Lite diffusion.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/highcolor/0091_byte_sierra1_neighborhood || {
    echo "not ok 1 - high-color Sierra Lite diffusion"
    exit 0
}
echo "ok 1 - high-color Sierra Lite diffusion"
exit 0
