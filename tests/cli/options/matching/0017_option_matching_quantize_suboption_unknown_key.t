#!/bin/sh
# TAP test verifying unknown -Q suboption keys are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qk:z=p "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown -Q suboption key unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"unknown suboption key"*)
        ;;
    *)
        echo "not ok" 1 - "missing unknown suboption key diagnostic"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

case "${msg}" in
    *"\"z\""*"valid keys"*"inittype"*)
        ;;
    *)
        echo "not ok" 1 - "missing token/candidate details for unknown -Q suboption key"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "unknown -Q suboption key is rejected"
exit 0
