#!/bin/sh
# TAP test: pal-riff prefix overrides mismatched .gpl extension.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
riff_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/riff-valid-with-gpl-extension.gpl"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin -m pal-riff:"${riff_palette}" \
    "${snake_png}" -o/dev/null || {
    echo "not ok" 1 - "pal-riff prefix should override extension"
    exit 0
}

echo "ok" 1 - "pal-riff prefix overrides .gpl extension"

exit 0
