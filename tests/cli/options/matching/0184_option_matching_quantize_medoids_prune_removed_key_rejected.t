#!/bin/sh
# TAP test verifying removed medoids prune key is rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qmedoids:prune=elkan \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "removed medoids prune key unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing removed medoids prune key diagnostic"
    exit 0
}

test "${msg#*\"prune\"*}" != "${msg}" || {
    echo "not ok" 1 - "missing removed medoids prune key name"
    exit 0
}

echo "ok" 1 - "removed medoids prune key is rejected"
exit 0
