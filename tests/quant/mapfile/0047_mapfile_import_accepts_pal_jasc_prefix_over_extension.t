#!/bin/sh
# TAP test: pal-jasc prefix overrides mismatched .act extension.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-jasc-valid-with-act-extension.act"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin -m pal-jasc:"${pal_palette}" \
    "${snake_png}" -o/dev/null || {
    echo "not ok" 1 - "pal-jasc prefix should override extension"
    exit 0
}

echo "ok" 1 - "pal-jasc prefix overrides .act extension"

exit 0
