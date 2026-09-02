#!/bin/sh
# TAP test verifying kmeans feedback uses the shared boolean validator.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qk:Finvalid "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid kmeans feedback value succeeded"
    exit 0
}

test "${msg#*boolean suboption must be 0 or 1.*}" != "${msg}" || {
    echo "not ok" 1 - "missing shared boolean diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*:feedback=0|1*}" != "${msg}" || {
    echo "not ok" 1 - "missing feedback key diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "kmeans feedback uses the shared boolean validator"
exit 0
