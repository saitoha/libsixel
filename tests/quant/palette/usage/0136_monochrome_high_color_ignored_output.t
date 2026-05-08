#!/bin/sh
# Verify high-color mode does not change encoded output with monochrome mode.
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
        -e -o - "${input_image}"
) || {
    echo "not ok" 1 - "monochrome baseline encode failed"
    exit 0
}

actual_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -I -e -o - "${input_image}"
) || {
    echo "not ok" 1 - "monochrome encode failed with -I"
    exit 0
}

test "${actual_output}" = "${expected_output}" || {
    echo "not ok" 1 - "monochrome output changed under -I"
    exit 0
}

echo "ok" 1 - "monochrome output ignores high-color mode"
exit 0
