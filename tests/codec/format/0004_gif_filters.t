#!/bin/sh
# Validate GIF conversion with scaling and background.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"
target_sixel="${ARTIFACT_LOCAL_DIR}/snake-gif.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -w105% -h100 -B"#000000000" -rne <"${snake_gif}" >"${target_sixel}" || {
    echo "not ok" 1 - "GIF conversion with filters failed"
    exit 0
}

echo "ok" 1 - "GIF conversion with filters succeeded"

exit 0
