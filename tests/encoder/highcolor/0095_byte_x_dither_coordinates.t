#!/bin/sh
# Check decoded byte samples for high-color X-dither coordinate samples.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/highcolor/0095_byte_x_dither_coordinates || {
    printf '\n%s\n' "not ok 1 - high-color X-dither coordinate samples"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color X-dither coordinate samples"
exit 0
