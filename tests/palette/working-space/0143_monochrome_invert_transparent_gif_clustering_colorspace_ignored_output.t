#!/bin/sh
# Verify -X does not change inverted monochrome output for transparent GIF input.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose2.gif"

expected_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -g -L builtin! -Akeep -ldisable -e -i -o - "${input_image}"
) || {
    printf '\n%s\n' "not ok 1 - inverted monochrome transparent GIF baseline encode failed"
    exit 0
}

test -n "${expected_output}" || {
    printf '\n%s\n' "not ok 1 - inverted monochrome transparent GIF output is empty"
    exit 0
}

actual_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -g -L builtin! -Akeep -ldisable -X oklab -e -i -o - "${input_image}"
) || {
    printf '\n%s\n' "not ok 1 - inverted monochrome transparent GIF encode failed with -X oklab"
    exit 0
}

test "${actual_output}" = "${expected_output}" || {
    printf '\n%s\n' "not ok 1 - inverted monochrome transparent GIF output changed under -X oklab"
    exit 0
}

printf '\n%s\n' "ok 1 - inverted monochrome transparent GIF output ignores clustering colorspace"
exit 0
