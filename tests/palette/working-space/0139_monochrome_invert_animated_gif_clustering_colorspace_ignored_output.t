#!/bin/sh
# Verify -X does not change inverted monochrome output for GIF input.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop2.gif"

expected_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -g -L builtin! -ldisable -e -i -o - "${input_image}"
) || {
    echo "not ok" 1 - "inverted monochrome GIF baseline encode failed"
    exit 0
}

test -n "${expected_output}" || {
    echo "not ok" 1 - "inverted monochrome GIF output is empty"
    exit 0
}

actual_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -g -L builtin! -ldisable -X oklab -e -i -o - "${input_image}"
) || {
    echo "not ok" 1 - "inverted monochrome GIF encode failed with -X oklab"
    exit 0
}

test "${actual_output}" = "${expected_output}" || {
    echo "not ok" 1 - "inverted monochrome GIF output changed under -X oklab"
    exit 0
}

echo "ok" 1 - "inverted monochrome GIF output ignores clustering colorspace"
exit 0
