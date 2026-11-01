#!/bin/sh
# Verify lsqa accepts the compact Vlight dequantize alias.

# Option parsing needs no full-size image or unrelated quality metrics.
set -eux

test -n "${LSQA_PATH-}" || {
    printf "1..0 # SKIP lsqa is disabled in this build\n"
    exit 0
}

test -x "${LSQA_PATH}" || {
    printf "1..0 # SKIP lsqa is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${LSQA_PATH}" -m PSNR_Y -d l:Vl \
        "${TOP_SRCDIR}/tests/data/color-numeric/stable.six" \
        "${TOP_SRCDIR}/tests/data/color-numeric/stable.six" >/dev/null || {
    echo "not ok" 1 - "lsqa compact Vlight dequantize alias rejected"
    exit 0
}

echo "ok" 1 - "lsqa accepts compact Vlight dequantize alias"
exit 0
