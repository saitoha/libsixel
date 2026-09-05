#!/bin/sh
# Verify the removed Kmeans autoratio suboption is not accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:autoratio=32 2>&1) && {
    echo "not ok 1 - removed Kmeans autoratio suboption succeeded"
    exit 0
}

test "${message#*unknown suboption key*autoratio*}" != "${message}" || {
    echo "not ok 1 - missing removed Kmeans autoratio diagnostic"
    exit 0
}

echo "ok 1 - removed Kmeans autoratio suboption is rejected"
exit 0
