#!/bin/sh
# Check decoded byte samples for high-color Floyd-Steinberg saturation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/highcolor/0096_byte_fs_saturation || {
    printf '\n%s\n' "not ok 1 - high-color Floyd-Steinberg saturation"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color Floyd-Steinberg saturation"
exit 0
