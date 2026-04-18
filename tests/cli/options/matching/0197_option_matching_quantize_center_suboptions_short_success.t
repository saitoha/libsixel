#!/bin/sh
# TAP test verifying -Q accepts short-form center suboptions.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:a=fft:s=42:iter=8 \
    "${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png" \
    >/dev/null || {
    echo "not ok" 1 - "-Q center short suboptions were rejected"
    exit 0
}

echo "ok" 1 - "-Q accepts short center suboptions"
exit 0
