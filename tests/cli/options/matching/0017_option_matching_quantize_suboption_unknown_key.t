#!/bin/sh
# TAP test verifying unknown -Q suboption keys use CLI diagnostic code contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=''
diag_line=''
status=0
nl='
'

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=code \
    -Qk:z=p "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) || \
    status=$?

test "${status}" -eq 2 || {
    echo "not ok" 1 - "unknown -Q suboption key exit status mismatch"
    exit 0
}

diag_line=${msg%%"${nl}"*}

case "${diag_line}" in
    LSXCLI1\|phase=option_parse\|rc=*\|code=UNKNOWN_SUBOPTION_KEY)
        ;;
    *)
        echo "not ok" 1 - "unknown -Q suboption key diagnostic header mismatch"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "unknown -Q suboption key keeps RC+diagnostic code contract"
exit 0
