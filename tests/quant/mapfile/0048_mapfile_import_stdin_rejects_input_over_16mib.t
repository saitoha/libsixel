#!/bin/sh
# TAP test: stdin mapfile import rejects payloads above 16 MiB.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/gpl-over-16mib.gpl"

(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m gpl:- "${snake_png}" \
          -o/dev/null <"${gpl_palette}" >/dev/null 2>&1) && {
    echo "not ok" 1 - "stdin mapfile over 16 MiB unexpectedly succeeded"
    exit 0
}

echo "ok" 1 - "stdin mapfile over 16 MiB is rejected"

exit 0
