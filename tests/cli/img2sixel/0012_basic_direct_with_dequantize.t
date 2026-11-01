#!/bin/sh
# TAP test producing direct RGBA output after dequantization.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D -dk_undither \
        <"${TOP_SRCDIR}/tests/data/inputs/snake_64.six" >/dev/null || {
    echo "not ok" 1 - "direct dequantize conversion failed"
    exit 0
}

echo "ok" 1 - "produces direct RGBA output after dequantize"
exit 0
