#!/bin/sh
# TAP test: RIFF import rejects data chunks with entry_count=257.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
riff_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/riff-entry-count-257.pal"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m pal-riff:"${riff_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "RIFF entry_count=257 unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_pal_riff: invalid entry count.}" != "${msg}" || {
    echo "not ok" 1 - "missing RIFF entry_count=257 diagnostic"
    exit 0
}

echo "ok" 1 - "RIFF entry_count=257 is rejected"

exit 0
