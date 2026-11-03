#!/bin/sh
# Scale with Lanczos3 filter using a two-colour palette.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 2 -h100 -wauto -rlanczos3 "${snake_jpg}" \
        -o/dev/null || {
    echo "not ok" 1 - "Lanczos3 scaling with two-colour palette fails"
    exit 0
}

echo "ok" 1 - "Lanczos3 scaling with two-colour palette works"

exit 0
