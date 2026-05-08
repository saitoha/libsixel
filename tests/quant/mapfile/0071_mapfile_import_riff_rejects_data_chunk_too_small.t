#!/bin/sh
# TAP test: RIFF PAL import rejects undersized data chunks.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
riff_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/riff-data-too-small.pal"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m pal-riff:"${riff_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "RIFF undersized-data payload unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_pal_riff: data chunk too small.}" != "${msg}" || {
    echo "not ok" 1 - "missing RIFF data-chunk-too-small diagnostic"
    exit 0
}

echo "ok" 1 - "RIFF undersized-data payload is rejected"

exit 0
