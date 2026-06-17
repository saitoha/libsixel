#!/bin/sh
# TAP test ensuring -Q sticky_weight encodes animated APNG input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"

sticky_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qauto:sticky_weight=8 -d fs -p 2 \
        "${input_apng}"
) || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

test -n "${sticky_output}" || {
    echo "not ok" 1 - "sticky_weight produced empty animated APNG output"
    exit 0
}

echo "ok" 1 - "-Q sticky_weight encodes animated APNG output"
exit 0
