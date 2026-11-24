#!/bin/sh
# Verify 6bit lookup accepts shared_instance suboption.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --lookup-policy=6bit:shared_instance=1 \
    -p 16 \
    "${input_image}" >/dev/null || {
    printf '\n%s\n' "not ok 1 - 6bit shared_instance suboption was rejected"
    exit 0
}

printf '\n%s\n' "ok 1 - 6bit shared_instance suboption accepted"

exit 0
