#!/bin/sh
# TAP test verifying -Q kmeans accepts quality knobs and merge suboptions.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=7:restarts=2:iter=8:iter_max=10:miniter=1:polish_iter=2:feedback_slots=2:feedback_interval=2:merge=ward:merge_oversplit=1.5:merge_lloyd=3 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "kmeans quality and merge suboptions were rejected"
    exit 0
}

echo "ok" 1 - "kmeans quality and merge suboptions are accepted"
exit 0
