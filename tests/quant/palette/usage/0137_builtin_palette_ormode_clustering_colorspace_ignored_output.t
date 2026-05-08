#!/bin/sh
# Verify -X does not change ormode encoded output with built-in palettes.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

expected_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -O -bxterm16 -o - "${input_image}"
) || {
    echo "not ok" 1 - "builtin palette ormode baseline encode failed"
    exit 0
}

test -n "${expected_output}" || {
    echo "not ok" 1 - "builtin palette ormode output is empty"
    exit 0
}

actual_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -O -X oklab -bxterm16 -o - "${input_image}"
) || {
    echo "not ok" 1 - "builtin palette ormode encode failed with -X oklab"
    exit 0
}

test "${actual_output}" = "${expected_output}" || {
    echo "not ok" 1 - "builtin palette ormode output changed under -X oklab"
    exit 0
}

echo "ok" 1 - "built-in palette ormode output ignores clustering colorspace"
exit 0
