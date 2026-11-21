#!/bin/sh
# Check decoded byte samples for high-color Sierra Two-row diffusion.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/highcolor/0092_byte_sierra2_neighborhood || {
    echo "not ok 1 - high-color Sierra Two-row diffusion"
    exit 0
}
echo "ok 1 - high-color Sierra Two-row diffusion"
exit 0
