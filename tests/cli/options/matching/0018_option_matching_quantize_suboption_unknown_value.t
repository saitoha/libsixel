#!/bin/sh
# TAP test verifying unknown -Q suboption values are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qk:i=xyz "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown -Q suboption value unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"unknown suboption value"*)
        ;;
    *)
        echo "not ok" 1 - "missing unknown suboption value diagnostic"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

case "${msg}" in
    *"\"xyz\""*"\"inittype\""*"valid values"*"auto, none, pca"*)
        ;;
    *)
        echo "not ok" 1 - "missing token/candidate details for unknown -Q suboption value"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "unknown -Q suboption value is rejected"
exit 0
