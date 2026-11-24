#!/bin/sh
# Check decoded high-color samples after Floyd-Steinberg diffusion.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/highcolor/0086_byte_fs_neighborhood || {
    printf '\n%s\n' "not ok 1 - legacy high-color Floyd-Steinberg diffusion"
    exit 0
}
printf '\n%s\n' "ok 1 - legacy high-color Floyd-Steinberg diffusion"
exit 0
