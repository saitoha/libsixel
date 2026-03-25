#!/bin/sh
# TAP test verifying unknown libtiff -L suboption values are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

msg=$(set +xv; run_img2sixel -Llibtiff:cms_engine=2! \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.tiff" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown libtiff -L suboption value unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"unknown suboption value"*)
        ;;
    *)
        echo "not ok" 1 - "missing unknown libtiff -L suboption value diagnostic"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

case "${msg}" in
    *"\"2\""*"\"cms_engine\""*"valid values"*"none"*"auto"*"builtin"*"lcms2"*"colorsync"*)
        ;;
    *)
        echo "not ok" 1 - "missing token/candidate details for unknown libtiff -L suboption value"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "unknown libtiff -L suboption value is rejected"
exit 0
