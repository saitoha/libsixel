#!/bin/sh
# Crop Sixel input with large offsets tolerated.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

snake_six="${TOP_SRCDIR}/images/map8.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -c200x200+2000+2000 "${snake_six}" -o/dev/null || {
    echo "not ok" 1 - "Sixel cropping with large offsets fails"
    exit 0
}

echo "ok" 1 - "Sixel cropping tolerates large offsets"

exit 0
