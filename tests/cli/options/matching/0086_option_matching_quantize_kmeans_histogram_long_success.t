#!/bin/sh
# TAP test verifying -Q accepts long-form kmeans histogram suboptions.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:binning=soft:binbits=6:mapping=srgb:softdist=trilinear:autoratio=32:feedback=on \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >/dev/null || {
    echo "not ok" 1 - "-Q kmeans long histogram suboptions were rejected"
    exit 0
}

echo "ok" 1 - "-Q accepts long kmeans histogram suboptions"
exit 0
