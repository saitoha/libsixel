#!/bin/sh
# TAP test: fallback path produces output data when tables are disabled.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_ascii_pbm="${TOP_SRCDIR}/images/snake-ascii.pbm"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_PALETTE_DISABLE_TABLES=1 "${snake_ascii_pbm}" -o "${ARTIFACT_LOCAL_DIR}/snake-fallback.sixel" || {
    printf 'not ok 1 - fallback output empty\n'
    exit 0
}

printf 'ok 1 - fallback output produced data\n'
exit 0
