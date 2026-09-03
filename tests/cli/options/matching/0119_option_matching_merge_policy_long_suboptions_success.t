#!/bin/sh
# TAP test verifying merge-policy long suboptions work independently of -Q.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qauto -Fward:merge_oversplit=1.2:merge_lloyd=0:channel_l=0.5 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "ward merge-policy suboptions were rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qheckbert -Fnone:merge_oversplit=1.8:merge_lloyd=3:channel_l=0.4 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "none merge-policy suboptions were rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qmedoids:algo=sample \
    -Fauto:merge_oversplit=2.0:merge_lloyd=5:channel_l=0.3 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "auto merge-policy suboptions were rejected"
    exit 0
}

echo "ok" 1 - "merge-policy long suboptions are accepted"
exit 0
