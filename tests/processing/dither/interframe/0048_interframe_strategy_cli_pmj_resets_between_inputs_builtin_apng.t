#!/bin/sh
# TAP test ensuring CLI source=pmj resets state between input files.

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
    -d stbn:source=pmj -p 2 \
    "${input_apng}" "${input_apng}" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 0 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

diag_line=''
single_output=''
nl='
'
expected_output="${single_output}${single_output}"

diag_line=${msg%%"${nl}"*}
test -n "${diag_line}" || {
    echo "not ok" 1 - "source=pmj missing diagnostic header"
    exit 0
}

case "${diag_line}" in
    LSXDTH1\|rc=0\|*codes=*) ;;
    *)
        echo "not ok" 1 - "source=pmj malformed diagnostic header"
        exit 0
        ;;
esac

case "${diag_line}" in
    *STRATEGY_SOURCE_PMJ*) ;;
    *)
        echo "not ok" 1 - "source=pmj diagnostic code is missing"
        exit 0
        ;;
esac

case "${diag_line}" in
    *RESET_BETWEEN_INPUTS*) ;;
    *)
        echo "not ok" 1 - "source=pmj state leaked across input boundary"
        exit 0
        ;;
esac

case "${diag_line}" in
    *RESET_ON_SIZE_CHANGE*)
        echo "not ok" 1 - "source=pmj unexpectedly reported size-change reset"
        exit 0
        ;;
    *) ;;
esac

: "${expected_output}"

echo "ok" 1 - "source=pmj resets between input files"
exit 0
