#!/bin/sh
# TAP test ensuring stbn alpha_guard changes float32 builtin apng output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_apng_12x8_rgba_loop2.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    -L builtin \
    -ldisable \
    --precision=float32 \
    -S -T 1 \
    -d fs -p 16 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP float32 animated builtin APNG frame path unavailable\n"
    exit 0
}

echo "1..1"
set -v

base_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        --precision=float32 \
        -d stbn:source=mask -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "float32 stbn mask base encode failed"
    exit 0
}

guard_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        --precision=float32 \
        -d stbn:source=mask:alpha_guard=1 -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "float32 stbn alpha_guard encode failed"
    exit 0
}

test "${guard_output}" != "${base_output}" || {
    echo "not ok" 1 - "float32 stbn alpha_guard did not change output"
    exit 0
}

echo "ok" 1 - "float32 stbn alpha_guard changes output"
exit 0
