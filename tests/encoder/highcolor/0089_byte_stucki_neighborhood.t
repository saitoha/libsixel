#!/bin/sh
# Check decoded byte samples for high-color Stucki diffusion.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/highcolor/0089_byte_stucki_neighborhood || {
    echo "not ok 1 - high-color Stucki diffusion"
    exit 0
}
echo "ok 1 - high-color Stucki diffusion"
exit 0
