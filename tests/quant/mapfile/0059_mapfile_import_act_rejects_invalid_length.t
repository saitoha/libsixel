#!/bin/sh
# TAP test: ACT import rejects invalid non-record-aligned lengths.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/riff-entry-count-257.pal"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m act:"${act_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid-length ACT palette unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_act: invalid ACT length.}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid ACT length diagnostic"
    exit 0
}

echo "ok" 1 - "invalid-length ACT palette is rejected"

exit 0
