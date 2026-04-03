#!/bin/sh
# TAP test verifying unknown -Q kmeans keys list histogram valid keys.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qk:z=soft "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown -Q key unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing unknown suboption key diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*valid keys*binning*mapping*autoratio*feedback*}" != "${msg}" || {
    echo "not ok" 1 - "missing histogram key list in unknown key diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "unknown -Q key diagnostic includes histogram keys"
exit 0
