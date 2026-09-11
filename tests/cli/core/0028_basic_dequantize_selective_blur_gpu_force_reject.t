#!/bin/sh
# Verify forced GPU dequantize rejects selective_blur instead of CPU fallback.
# Policy: docs/functionality/dequantization.md

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" --env SIXEL_GPU_POLICY=force \
        -ds:T24 <"${TOP_SRCDIR}/images/map8.six" >/dev/null 2>&1 && {
    echo "not ok" 1 - "forced GPU selective_blur fell back to CPU"
    exit 0
}

echo "ok" 1 - "forced GPU selective_blur is rejected"
exit 0
