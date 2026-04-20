#!/bin/sh
# TAP test ensuring interframe diffusion state is reset between input files.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP webp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_anim="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-loop2-min.webp"
msg=''
diag_line=''
status=0
nl='
'

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=dither_contract \
    --threads=1 -ldisable \
    -d interframe -p 2 \
    "${input_anim}" "${input_anim}" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 0 || {
    echo "not ok" 1 - "interframe two-input encode failed"
    exit 0
}

diag_line=${msg%%"${nl}"*}
test -n "${diag_line}" || {
    echo "not ok" 1 - "interframe missing diagnostic header"
    exit 0
}

case "${diag_line}" in
    LSXDTH1\|rc=0\|*codes=*) ;;
    *)
        echo "not ok" 1 - "interframe malformed diagnostic header"
        exit 0
        ;;
esac

case "${diag_line}" in
    *RESET_BETWEEN_INPUTS*) ;;
    *)
        echo "not ok" 1 - "interframe missing RESET_BETWEEN_INPUTS contract"
        exit 0
        ;;
esac

echo "ok" 1 - "interframe resets between input files"
exit 0
