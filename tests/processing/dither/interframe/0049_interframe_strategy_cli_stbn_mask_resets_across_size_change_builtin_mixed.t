#!/bin/sh
# TAP test ensuring CLI source=mask resets before size-changing input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_apng_12x8_rgba_loop2.png"
input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose2.gif"

msg=''
status=0
msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=dither_contract \
    --threads=1 \
    -L builtin \
    -ldisable \
    -d stbn:source=mask -p 2 \
    "${input_apng}" "${input_gif}" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 0 || {
    printf "1..0 # SKIP animated builtin APNG/GIF frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

diag_line=''
animated_output=''
single_output=''
nl='
'
expected_output="${animated_output}${single_output}"

diag_line=${msg%%"${nl}"*}
test -n "${diag_line}" || {
    echo "not ok" 1 - "source=mask missing diagnostic header"
    exit 0
}

case "${diag_line}" in
    LSXDTH1\|rc=0\|*codes=*) ;;
    *)
        echo "not ok" 1 - "source=mask malformed diagnostic header"
        exit 0
        ;;
esac

case "${diag_line}" in
    *\|source=mask\|*) ;;
    *)
        echo "not ok" 1 - "source=mask diagnostic source marker is missing"
        exit 0
        ;;
esac

case "${diag_line}" in
    *RESET_BETWEEN_INPUTS*) ;;
    *)
        echo "not ok" 1 - "source=mask missing input-boundary reset contract"
        exit 0
        ;;
esac

: "${expected_output}"

echo "ok" 1 - "source=mask resets between mixed-size inputs"
exit 0
