#!/bin/sh
# TAP test verifying sticky does not accept the legacy animation_mode key.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=''
diag_line=''
diag_code=''
status=0
nl='
'

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=code \
    --env SIXEL_DIAG_MODE_QUIET=1 \
    -Qsticky:animation_mode=1 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) || \
    status=$?

test "${status}" -eq 2 || {
    echo "not ok" 1 - "sticky animation_mode exit status mismatch"
    exit 0
}

diag_line=${msg%%"${nl}"*}
diag_code=${diag_line#*|code=}

test "${diag_line#LSXCLI1|phase=option_parse|rc=}" != "${diag_line}" || {
    echo "not ok" 1 - "sticky animation_mode diagnostic prefix mismatch"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${diag_code#UNKNOWN_SUBOPTION_KEY}" != "${diag_code}" || {
    echo "not ok" 1 - "sticky animation_mode diagnostic code mismatch"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "sticky rejects legacy animation_mode suboption"
exit 0
