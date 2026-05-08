#!/bin/sh
# Verify explicit dither methods apply with built-in palettes.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

none_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -bxterm16 -d none -o - "${input_image}"
) || {
    echo "not ok" 1 - "builtin palette encode failed with -d none"
    exit 0
}

fs_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -bxterm16 -d fs -o - "${input_image}"
) || {
    echo "not ok" 1 - "builtin palette encode failed with -d fs"
    exit 0
}

test -n "${none_output}" || {
    echo "not ok" 1 - "builtin palette -d none output is empty"
    exit 0
}
test -n "${fs_output}" || {
    echo "not ok" 1 - "builtin palette -d fs output is empty"
    exit 0
}
test "${none_output}" != "${fs_output}" || {
    echo "not ok" 1 - "builtin palette dither methods produced identical output"
    exit 0
}

echo "ok" 1 - "built-in palette output changes under explicit dither methods"
exit 0
