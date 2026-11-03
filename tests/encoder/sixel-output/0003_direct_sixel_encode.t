#!/bin/sh
# Encode existing Sixel data directly.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

snake_six="${TOP_SRCDIR}/images/map8.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -e "${snake_six}" -o/dev/null || {
    echo "not ok" 1 - "direct Sixel encode failed"
    exit 0
}

echo "ok" 1 - "direct Sixel encode emits data"
exit 0
