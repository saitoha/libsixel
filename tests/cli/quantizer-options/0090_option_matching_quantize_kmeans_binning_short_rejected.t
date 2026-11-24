#!/bin/sh
# Verify Kmeans no longer accepts the compact binning suboption.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:Bsoft 2>&1) && {
    printf '\n%s\n' "not ok 1 - removed compact Kmeans binning suboption succeeded"
    exit 0
}

case "${message}" in *unknown\ suboption\ key*Bsoft**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - missing compact Kmeans binning diagnostic"
    exit 0
}

printf '\n%s\n' "ok 1 - removed compact Kmeans binning suboption is rejected"
exit 0
