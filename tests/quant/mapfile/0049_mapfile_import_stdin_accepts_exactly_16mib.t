#!/bin/sh
# TAP test: stdin mapfile import accepts payloads at exactly 16 MiB.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/gpl-exact-16mib.gpl"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin -m gpl:- \
    "${snake_png}" -o/dev/null <"${gpl_palette}" || {
    echo "not ok" 1 - "stdin mapfile at 16 MiB should be accepted"
    exit 0
}

echo "ok" 1 - "stdin mapfile at 16 MiB is accepted"

exit 0
