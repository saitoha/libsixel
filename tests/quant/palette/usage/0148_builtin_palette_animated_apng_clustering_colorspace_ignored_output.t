#!/bin/sh
# Verify -X does not change built-in palette output for APNG input.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"

expected_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -g -L builtin! -bxterm16 -o - "${input_image}"
) || {
    echo "not ok" 1 - "builtin palette APNG baseline encode failed"
    exit 0
}

test -n "${expected_output}" || {
    echo "not ok" 1 - "builtin palette APNG output is empty"
    exit 0
}

actual_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -g -L builtin! -X oklab -bxterm16 -o - "${input_image}"
) || {
    echo "not ok" 1 - "builtin palette APNG encode failed with -X oklab"
    exit 0
}

test "${actual_output}" = "${expected_output}" || {
    echo "not ok" 1 - "builtin palette APNG output changed under -X oklab"
    exit 0
}

echo "ok" 1 - "built-in palette APNG output ignores clustering colorspace"
exit 0
