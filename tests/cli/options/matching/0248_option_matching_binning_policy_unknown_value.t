#!/bin/sh
# Verify the quantize binning_policy suboption rejects an unknown value.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qauto:binning_policy=unknown \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - unknown binning-policy policy exit status mismatch"
    exit 0
}

test "${message#*binning_policy*}" != "${message}" || {
    echo "not ok 1 - unknown binning-policy diagnostic is missing"
    exit 0
}

echo "ok 1 - quantize binning_policy rejects an unknown value"
exit 0
