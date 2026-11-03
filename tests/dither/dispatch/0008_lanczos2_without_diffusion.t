#!/bin/sh
# Scale with Lanczos2 filter while disabling diffusion.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -p 8 -h200 -fnorm -rlanczos2 -dnone \
        "${snake_jpg}" -o/dev/null || {
    echo "not ok" 1 - "Lanczos2 scaling without diffusion fails"
    exit 0
}

echo "ok" 1 - "Lanczos2 scaling without diffusion succeeds"

exit 0
