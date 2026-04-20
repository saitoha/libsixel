#!/bin/sh
# TAP test verifying unknown stbn suboption keys are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_DIAG_MODE=code \
        --env SIXEL_DIAG_MODE_QUIET=1 \
        -d stbn:mode=hash \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "unknown stbn key unexpectedly succeeded"
    exit 0
}

diag_line=${msg%%'
'*}

case "${diag_line}" in
    LSXCLI1\|phase=option_parse\|rc=*\|code=UNKNOWN_SUBOPTION_KEY) ;;
    *)
        echo "not ok" 1 - "missing unknown stbn key diagnostic code"
        exit 0
        ;;
esac

case "${diag_line}" in
    *\|rc=0\|*)
    echo "not ok" 1 - "unknown stbn key unexpectedly reported rc=0"
    exit 0
    ;;
esac

echo "ok" 1 - "unknown stbn key is rejected"
exit 0
