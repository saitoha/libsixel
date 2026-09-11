#!/bin/sh
# Policy: docs/cli/suboptions.md
# Policy: docs/functionality/dequantization.md
# Verify lsqa accepts selective_blur full and compact forms.

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

${SIXEL_RUNTIME-} "${LSQA_PATH}" -d selective_blur:threshold=24 \
        "${TOP_SRCDIR}/images/snake.six" \
        "${TOP_SRCDIR}/images/snake.six" >/dev/null || {
    echo "not ok" 1 - "lsqa selective_blur full form rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${LSQA_PATH}" -d s:T24 \
        "${TOP_SRCDIR}/images/snake.six" \
        "${TOP_SRCDIR}/images/snake.six" >/dev/null || {
    echo "not ok" 1 - "lsqa selective_blur compact form rejected"
    exit 0
}

echo "ok" 1 - "lsqa accepts selective_blur full and compact forms"
exit 0
