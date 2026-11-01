#!/bin/sh
# TAP test verifying unknown kmeans mapping values are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qk:Minvalid "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown kmeans mapping value unexpectedly succeeded"
    exit 0
}

case "${msg}" in *unknown\ suboption\ value**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "missing unknown suboption value diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in *\"mapping\"**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "missing mapping key diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in *valid\ values*uniform,\ srgb**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "missing kmeans mapping valid value list"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "unknown kmeans mapping value is rejected"
exit 0
