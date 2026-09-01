#!/bin/sh
# Verify the registered selective-blur threshold environment value is applied.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

from_env=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env SIXEL_DEQUANTIZE_SELECTIVE_BLUR_THRESHOLD=36 \
    -dselective_blur <"${TOP_SRCDIR}/images/map8.six" | cksum)
from_cli=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    -dselective_blur:T36 \
    <"${TOP_SRCDIR}/images/map8.six" | cksum)
test "${from_env}" = "${from_cli}" || {
    echo "not ok" 1 - "selective-blur threshold environment changed output"
    exit 0
}

echo "ok" 1 - "selective-blur threshold environment value is applied"
exit 0
