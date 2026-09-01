#!/bin/sh
# TAP test verifying long -Q suboption key prefixes are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=''
diag_line=''
diag_tail=''
status=0
nl='
'

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=code \
    --env SIXEL_DIAG_MODE_QUIET=1 \
    -Qk:init=pca "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || status=$?

test "${status}" -eq 2 || {
    echo "not ok" 1 - "long -Q suboption prefix exit status mismatch"
    exit 0
}

diag_line=${msg%%"${nl}"*}
diag_tail=${diag_line#LSXCLI1|phase=option_parse|rc=*|code=UNKNOWN_SUBOPTION_KEY}

test "${diag_tail}" != "${diag_line}" || {
    echo "not ok" 1 - "long -Q suboption prefix diagnostic mismatch"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "long -Q suboption key prefix is rejected"
exit 0
