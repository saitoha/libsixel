#!/bin/sh
# Check decoded byte samples for high-color Jarvis, Judice and Ninke diffusion.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/highcolor/0088_byte_jajuni_neighborhood || {
    echo "not ok 1 - high-color Jarvis, Judice and Ninke diffusion"
    exit 0
}
echo "ok 1 - high-color Jarvis, Judice and Ninke diffusion"
exit 0
