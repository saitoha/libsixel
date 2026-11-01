#!/bin/sh
# Ensure xterm palette overrides can repeat safely.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_pbm="${TOP_SRCDIR}/tests/data/inputs/snake_64.pbm"
target_sixel="${ARTIFACT_LOCAL_DIR}/xterm-override.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 -w100 -h100 -bxterm16 -B"#aB3" -B"#aB3" \
        "${snake_pbm}" >"${target_sixel}" || {
    echo "not ok" 1 - "xterm palette overrides fail"
    exit 0
}

echo "ok" 1 - "xterm palette overrides repeat"

exit 0
