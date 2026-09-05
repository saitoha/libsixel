#!/bin/sh
# Verify the long-only binning-policy option requires an argument.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --binning-policy --threads=1 2>&1) || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - missing binning-policy argument exit status mismatch"
    exit 0
}

test "${message#*--binning-policy*}" != "${message}" || {
    echo "not ok 1 - missing binning-policy diagnostic is missing"
    exit 0
}

echo "ok 1 - binning-policy requires an argument"
exit 0
