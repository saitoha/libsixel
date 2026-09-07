#!/bin/sh
# Verify only the canonical alpha-policy spellings are accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/small.ppm"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --alpha-policy=background \
    -o /dev/null "${input_image}" >/dev/null 2>&1 && {
    echo "not ok 1 - alpha-policy accepted background"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --alpha-policy=transparent \
    -o /dev/null "${input_image}" >/dev/null 2>&1 && {
    echo "not ok 1 - alpha-policy accepted transparent"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --alpha-policy=p2-0 \
    -o /dev/null "${input_image}" >/dev/null 2>&1 && {
    echo "not ok 1 - alpha-policy accepted p2-0"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --transparent-policy=keep \
    -o /dev/null "${input_image}" >/dev/null 2>&1 && {
    echo "not ok 1 - img2sixel accepted --transparent-policy"
    exit 0
}

echo "ok 1 - alpha-policy rejects unreleased names"
exit 0
