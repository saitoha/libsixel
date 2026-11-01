#!/bin/sh
# TAP test verifying repeated built-in palette options use last occurrence.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

expected_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -L builtin -bxterm16 -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "xterm16 reference palette export failed"
    exit 0
}
expected_palette=$(printf "%s" "${expected_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "xterm16 reference palette normalization failed"
    exit 0
}

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -L builtin -bxterm256 -bxterm16 -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "repeated built-in palette export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "repeated built-in palette normalization failed"
    exit 0
}

test -n "${actual_palette}" || {
    echo "not ok" 1 - "repeated built-in palette output is empty"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "last built-in palette did not take effect"
    exit 0
}

echo "ok" 1 - "repeated built-in palette follows last occurrence"
exit 0
