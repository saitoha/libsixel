#!/bin/sh
# Verify lsqa accepts the compact Vlight dequantize alias.

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

${SIXEL_RUNTIME-} "${LSQA_PATH}" -d l:Vl \
        "${TOP_SRCDIR}/images/snake.six" \
        "${TOP_SRCDIR}/images/snake.six" >/dev/null || {
    echo "not ok" 1 - "lsqa compact Vlight dequantize alias rejected"
    exit 0
}

echo "ok" 1 - "lsqa accepts compact Vlight dequantize alias"
exit 0
