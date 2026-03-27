#!/bin/sh
# Verify long option forms are accepted.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --height=100 --diffusion=atkinson     --outfile=/dev/null <"${snake_jpg}" || {
    echo "not ok" 1 - "long option forms rejected"
    exit 0
}

echo "ok" 1 - "long option forms accepted"
exit 0
