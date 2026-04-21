#!/bin/sh
# TAP test ensuring 8bit PMJ strategy contract differs from STBN hash.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"

hash_msg=''
status=0
hash_msg=$(set +xv; SIXEL_DITHER_STBN_SOURCE=stbn-hash \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_TRACE_TOPIC=dither_contract \
        --threads=1 \
        -L builtin \
        -ldisable \
        -S -T 1 \
        -d stbn -p 16 \
        "${input_apng}" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 0 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

pmj_msg=''
status=0
pmj_msg=$(set +xv; SIXEL_DITHER_STBN_SOURCE=pmj \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_TRACE_TOPIC=dither_contract \
        --threads=1 \
        -L builtin \
        -ldisable \
        -S -T 1 \
        -d stbn -p 16 \
        "${input_apng}" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 0 || {
    echo "not ok" 1 - "stbn pmj 8bit APNG encode failed"
    exit 0
}

hash_output=''
pmj_output=''
nl='
'

hash_output=${hash_msg%%"${nl}"*}
pmj_output=${pmj_msg%%"${nl}"*}

case "${hash_output}" in
    LSXDTH1\|rc=0\|*codes=*) ;;
    *)
        echo "not ok" 1 - "stbn-hash malformed diagnostic header"
        exit 0
        ;;
esac

case "${pmj_output}" in
    LSXDTH1\|rc=0\|*codes=*) ;;
    *)
        echo "not ok" 1 - "stbn pmj malformed diagnostic header"
        exit 0
        ;;
esac

case "${hash_output}" in
    *\|source=hash\|*) ;;
    *)
        echo "not ok" 1 - "stbn-hash source marker is missing"
        exit 0
        ;;
esac

case "${hash_output}" in
    *STRATEGY_SOURCE_PMJ*)
        echo "not ok" 1 - "stbn-hash unexpectedly reported pmj code"
        exit 0
        ;;
    *) ;;
esac

case "${pmj_output}" in
    *\|source=pmj\|*) ;;
    *)
        echo "not ok" 1 - "stbn pmj source marker is missing"
        exit 0
        ;;
esac

case "${pmj_output}" in
    *STRATEGY_SOURCE_PMJ*) ;;
    *)
        echo "not ok" 1 - "stbn pmj diagnostic code is missing"
        exit 0
        ;;
esac

test "${pmj_output}" != "${hash_output}" || {
    echo "not ok" 1 - "pmj strategy contract matched stbn-hash"
    exit 0
}

echo "ok" 1 - "8bit pmj strategy contract differs from stbn-hash"
exit 0
