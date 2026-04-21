#!/bin/sh
# TAP test ensuring interframe diffusion resets before size-changing input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP webp loader is unavailable\n"
    exit 0
}

input_anim="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_anim_12x8.webp"
input_gif="${TOP_SRCDIR}/tests/data/inputs/snake_64.gif"

msg=''
status=0
msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=dither_contract \
    --threads=1 -ldisable \
    -S -T 1 \
    -d interframe -p 2 \
    "${input_anim}" "${input_gif}" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 0 || {
    printf "1..0 # SKIP interframe mixed WEBP/GIF path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

diag_line=''
nl='
'

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
        echo "not ok" 1 - "interframe missing input-boundary reset contract"
        exit 0
        ;;
esac

echo "ok" 1 - "interframe resets across size change"
exit 0
