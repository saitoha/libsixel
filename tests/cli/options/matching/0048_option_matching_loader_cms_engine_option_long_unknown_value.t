#!/bin/sh
# TAP test verifying --cms-engine unknown values use code contract.

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
    --cms-engine=foo \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null 2>&1) || status=$?

test "${status}" -eq 2 || {
    echo "not ok" 1 - "unknown --cms-engine long value exit status mismatch"
    exit 0
}

diag_line=${msg%%"${nl}"*}

case "${diag_line}" in
    LSXCLI1\|phase=option_parse\|rc=*\|code=INVALID_CMS_ENGINE)
        ;;
    *)
        echo "not ok" 1 - \
            "unknown long --cms-engine diagnostic header mismatch"
        exit 0
        ;;
esac

echo "ok" 1 - "unknown long --cms-engine value keeps RC+diagnostic contract"
exit 0
