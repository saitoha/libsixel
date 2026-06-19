#!/bin/sh
# TAP test verifying sticky remains visible in the palette contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
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
    --env SIXEL_TRACE_TOPIC=palette_contract \
    -Qsticky \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) || \
    status=$?

test "${status}" -eq 0 || {
    echo "not ok" 1 - "sticky palette contract run failed"
    exit 0
}

diag_line=${msg%%"${nl}"*}

test "${diag_line#LSXPAL1*rc=0*}" != "${diag_line}" || {
    echo "not ok" 1 - "sticky palette contract header is malformed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${diag_line#*model=sticky*}" != "${diag_line}" || {
    echo "not ok" 1 - "sticky palette contract model name is missing"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${diag_line#*MODEL_STICKY*}" != "${diag_line}" || {
    echo "not ok" 1 - "sticky palette contract code is missing"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "sticky remains visible in palette contract"
exit 0
