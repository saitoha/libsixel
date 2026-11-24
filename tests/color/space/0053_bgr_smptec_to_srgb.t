#!/bin/sh
# Convert BGR SMPTE-C samples to the expected gamma sRGB bytes.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" colorspace/0053_bgr_smptec_to_srgb || {
    printf '\n%s\n' "not ok 1 - BGR SMPTE-C conversion produces the expected sRGB bytes"
    exit 0
}
printf '\n%s\n' "ok 1 - BGR SMPTE-C conversion produces the expected sRGB bytes"
exit 0
