#!/bin/sh
# TAP test validating --gri-limit preserves deterministic SIXEL output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +xv

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.png"
plain_output=''
limited_output=''

plain_output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -=1 "${input_image}") || {
    echo "not ok" 1 - "img2sixel plain encode failed"
    exit 0
}

limited_output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -=1 --gri-limit "${input_image}") || {
    echo "not ok" 1 - "img2sixel --gri-limit encode failed"
    exit 0
}

test -n "${plain_output}" || {
    echo "not ok" 1 - "plain SIXEL output is empty"
    exit 0
}

test "${plain_output}" = "${limited_output}" || {
    echo "not ok" 1 - "gri-limit deterministic output mismatch"
    exit 0
}

echo "ok" 1 - "gri-limit deterministic output matches"
exit 0
