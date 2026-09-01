#!/bin/sh
# TAP test verifying repeated monochrome options are accepted.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin -e -e -o/dev/null "${input_image}" || {
    echo "not ok" 1 - "repeated monochrome option failed"
    exit 0
}

echo "ok" 1 - "repeated monochrome option is accepted"
exit 0
