#!/bin/sh
# TAP test verifying common merge suboptions work on all quantize models.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qauto:merge=ward:merge_oversplit=1.2:merge_lloyd=0 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "auto merge suboptions were rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qheckbert:merge=none:merge_oversplit=1.8:merge_lloyd=3 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "heckbert merge suboptions were rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qmedoids:algo=sample:merge=auto:merge_oversplit=2.0:merge_lloyd=5 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "medoids merge suboptions were rejected"
    exit 0
}

echo "ok" 1 - "merge suboptions are accepted on auto/heckbert/medoids"
exit 0
