#!/bin/sh
# TAP test verifying -Q rejects non-numeric scene_cut_threshold values.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Qheckbert:scene_cut_threshold=invalid \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "non-numeric scene_cut_threshold unexpectedly succeeded"
    exit 0
}

test "${msg#*-Q scene_cut_threshold must be in range 0.0-1.0.*}" != "${msg}" \
    || {
    echo "not ok" 1 - "missing non-numeric scene_cut_threshold diagnostic"
    exit 0
}

echo "ok" 1 - "-Q rejects non-numeric scene_cut_threshold values"
exit 0
