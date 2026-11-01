#!/bin/sh
# TAP test verifying unknown -Q base tokens use CLI code contract.

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
    --env SIXEL_DIAG_MODE_QUIET=1 \
    -Qzzzmodel "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) || \
    status=$?

test "${status}" -eq 2 || {
    echo "not ok" 1 - "unknown -Q base token exit status mismatch"
    exit 0
}

diag_line=${msg%%"${nl}"*}

case "${diag_line}" in
    LSXCLI1\|phase=option_parse\|rc=*\|code=UNKNOWN_BASE_VALUE*)
        ;;
    *)
        echo "not ok" 1 - "unknown -Q base token diagnostic header mismatch"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "unknown -Q base token keeps RC+diagnostic code contract"
exit 0
