#!/bin/sh
# Verify the quantize sampling_policy suboption requires a value.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qauto:sampling_policy= --threads=1 2>&1) || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - missing sampling-policy argument exit status mismatch"
    exit 0
}

test "${message#*sampling_policy*}" != "${message}" || {
    echo "not ok 1 - missing sampling_policy value diagnostic is missing"
    exit 0
}

echo "ok 1 - quantize sampling_policy requires a value"
exit 0
