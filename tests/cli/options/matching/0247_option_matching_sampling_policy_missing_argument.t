#!/bin/sh
# Verify the long-only sampling-policy option requires an argument.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --sampling-policy --threads=1 2>&1) || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - missing sampling-policy argument exit status mismatch"
    exit 0
}

test "${message#*--sampling-policy*}" != "${message}" || {
    echo "not ok 1 - missing sampling-policy diagnostic is missing"
    exit 0
}

echo "ok 1 - sampling-policy requires an argument"
exit 0
