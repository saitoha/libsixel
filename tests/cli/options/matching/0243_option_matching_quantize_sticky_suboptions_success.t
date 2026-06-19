#!/bin/sh
# TAP test verifying -Q sticky accepts its Heckbert fallback suboptions.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qsticky:scene_cut_threshold=0.20:profile=speed:Gnone:O1.2:L2 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o /dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "-Q sticky suboptions were rejected"
    exit 0
}

echo "ok" 1 - "-Q accepts sticky suboptions"
exit 0
