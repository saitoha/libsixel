#!/bin/sh
# TAP test ensuring interframe float32 accepts builtin APNG animation input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_apng_12x8_rgba_loop2.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    --precision=float32 \
    -L builtin \
    -ldisable \
    -S -T 1 \
    -d fs -p 16 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

interframe_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "interframe float32 animated encode failed"
    exit 0
}

test -n "${interframe_output}" || {
    echo "not ok" 1 - "interframe float32 animated output is empty"
    exit 0
}

echo "ok" 1 - "interframe float32 accepts animated input"
exit 0
