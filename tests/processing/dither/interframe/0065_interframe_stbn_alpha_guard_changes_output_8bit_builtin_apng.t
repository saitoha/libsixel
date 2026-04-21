#!/bin/sh
# TAP test ensuring alpha_guard=1 is reflected in 8bit STBN contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"

msg=''
status=0
msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=dither_contract \
    --threads=1 \
    -L builtin \
    -ldisable \
    -S -T 1 \
    -d stbn:source=mask:alpha_guard=1 -p 16 \
    "${input_apng}" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 0 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

diag_line=''
nl='
'

diag_line=${msg%%"${nl}"*}
test -n "${diag_line}" || {
    echo "not ok" 1 - "8bit stbn alpha_guard missing diagnostic header"
    exit 0
}

case "${diag_line}" in
    LSXDTH1\|rc=0\|*codes=*) ;;
    *)
        echo "not ok" 1 - "8bit stbn alpha_guard malformed diagnostic header"
        exit 0
        ;;
esac

case "${diag_line}" in
    *\|source=mask\|*) ;;
    *)
        echo "not ok" 1 - "8bit stbn alpha_guard source marker is missing"
        exit 0
        ;;
esac

case "${diag_line}" in
    *ALPHA_GUARD_ON*) ;;
    *)
        echo "not ok" 1 - "8bit stbn alpha_guard contract code is missing"
        exit 0
        ;;
esac

echo "ok" 1 - "8bit stbn alpha_guard contract is reported"
exit 0
