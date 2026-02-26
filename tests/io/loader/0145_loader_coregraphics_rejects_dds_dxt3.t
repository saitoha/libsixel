#!/bin/sh
# TAP test: coregraphics loader rejects DDS DXT3 input safely.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-dds-dxt3.dds"

run_img2sixel -L coregraphics! "${image_path}" >/dev/null && {
    fail 1 "coregraphics unexpectedly decoded DDS DXT3 input"
    exit 0
}

pass 1 "coregraphics rejects DDS DXT3 input safely"
exit 0
