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

combined_cksum=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d stbn:source=pmj -p 16 \
        "${input_apng}" "${input_gif}" | cksum
) || {
    echo "not ok" 1 - "source=pmj combined mixed-size encode failed"
    exit 0
}

expected_cksum=$(
    {
        ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
            --threads=1 \
            -L builtin \
            -ldisable \
            -d stbn:source=pmj -p 16 \
            "${input_apng}"
        ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
            --threads=1 \
            -L builtin \
            -ldisable \
            -d stbn:source=pmj -p 16 \
            "${input_gif}"
    } | cksum
) || {
    echo "not ok" 1 - "source=pmj expected mixed stream encode failed"
    exit 0
}

test "${combined_cksum}" = "${expected_cksum}" || {
    echo "not ok" 1 - "source=pmj state leaked across size change"
    exit 0
}

echo "ok" 1 - "source=pmj resets across size change"
exit 0
