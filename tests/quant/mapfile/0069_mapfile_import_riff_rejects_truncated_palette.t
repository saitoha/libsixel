#!/bin/sh
# TAP test: RIFF PAL import rejects truncated payloads.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
riff_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/riff-truncated.pal"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m pal-riff:"${riff_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "RIFF truncated payload unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_pal_riff: truncated palette.}" != "${msg}" || {
    echo "not ok" 1 - "missing RIFF truncated-palette diagnostic"
    exit 0
}

echo "ok" 1 - "RIFF truncated payload is rejected"

exit 0
