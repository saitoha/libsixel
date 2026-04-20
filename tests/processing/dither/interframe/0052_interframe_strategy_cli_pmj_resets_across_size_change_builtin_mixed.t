#!/bin/sh
# TAP test ensuring CLI source=pmj resets before size-changing input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_apng_12x8_rgba_loop2.png"
input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose2.gif"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    -L builtin \
    -ldisable \
    -S -T 1 \
    -d fs -p 16 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    -L builtin \
    -ldisable \
    -S -T 0 \
    -d fs -p 16 \
    "${input_gif}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin GIF frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

msg=''
diag_line=''
status=0
animated_output=''
single_output=''
nl='
'
expected_output="${animated_output}${single_output}"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=dither_contract \
    --threads=1 \
    -L builtin \
    -ldisable \
    -d stbn:source=pmj -p 16 \
    "${input_apng}" "${input_gif}" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 0 || {
    echo "not ok" 1 - "source=pmj combined mixed-size encode failed"
    exit 0
}

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
        echo "not ok" 1 - "source=pmj missing input-boundary reset contract"
        exit 0
        ;;
esac

case "${diag_line}" in
    *RESET_ON_SIZE_CHANGE*) ;;
    *)
        echo "not ok" 1 - "source=pmj state leaked across size change"
        exit 0
        ;;
esac

: "${expected_output}"

echo "ok" 1 - "source=pmj resets across size change"
exit 0
