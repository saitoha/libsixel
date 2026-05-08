#!/bin/sh
# TAP test: RIFF PAL import rejects data chunks whose size disagrees with entry count.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
riff_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/riff-unexpected-chunk-size.pal"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m pal-riff:"${riff_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "RIFF unexpected-chunk-size payload unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_pal_riff: unexpected chunk size.}" != "${msg}" || {
    echo "not ok" 1 - "missing RIFF unexpected-chunk-size diagnostic"
    exit 0
}

echo "ok" 1 - "RIFF unexpected-chunk-size payload is rejected"

exit 0
