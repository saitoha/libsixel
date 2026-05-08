#!/bin/sh
# TAP test verifying repeated mapfile options use last occurrence.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
first_mapfile="${TOP_SRCDIR}/images/map8.six"
last_mapfile="${TOP_SRCDIR}/images/map16.png"

expected_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -L builtin -m "${last_mapfile}" -M pal:- -o /dev/null \
        "${input_image}"
) || {
    echo "not ok" 1 - "last mapfile reference palette export failed"
    exit 0
}
expected_palette=$(printf "%s" "${expected_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "last mapfile reference normalization failed"
    exit 0
}

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -L builtin -m "${first_mapfile}" -m "${last_mapfile}" \
        -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "repeated mapfile palette export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "repeated mapfile palette normalization failed"
    exit 0
}

test -n "${actual_palette}" || {
    echo "not ok" 1 - "repeated mapfile palette output is empty"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "last mapfile did not take effect"
    exit 0
}

echo "ok" 1 - "repeated mapfile follows last occurrence"
exit 0
