#!/bin/sh
# Verify a mapfile followed by a palette size is rejected.
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
mapfile_palette="${TOP_SRCDIR}/images/map8.six"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
          -m "${mapfile_palette}" -p64 -o/dev/null "${input_image}" \
          2>&1 >/dev/null) && {
    echo "not ok" 1 - "mapfile followed by palette size unexpectedly succeeded"
    exit 0
}

test "${msg#*option -p, --colors conflicts with -m, --mapfile.}" \
    != "${msg}" || {
    echo "not ok" 1 - "missing mapfile and palette-size conflict diagnostic"
    exit 0
}

echo "ok" 1 - "mapfile followed by palette size is rejected"
exit 0
