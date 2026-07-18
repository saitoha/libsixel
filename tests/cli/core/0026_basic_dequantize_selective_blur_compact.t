#!/bin/sh
# Verify the compact selective_blur form matches the full spelling.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

selective_blur_full=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
        -dselective_blur:threshold=24 \
    <"${TOP_SRCDIR}/images/map8.six" | cksum)
selective_blur_short=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -ds:T24 \
    <"${TOP_SRCDIR}/images/map8.six" | cksum)
test "${selective_blur_full}" = "${selective_blur_short}" || {
    echo "not ok" 1 - "compact selective_blur form changed output"
    exit 0
}

echo "ok" 1 - "compact selective_blur form matches full spelling"
exit 0
