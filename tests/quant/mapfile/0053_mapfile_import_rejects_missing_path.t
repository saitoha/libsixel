#!/bin/sh
# TAP test: missing mapfile paths are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
missing_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/missing-mapfile.pal"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m "${missing_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "missing mapfile path unexpectedly succeeded"
    exit 0
}

test "${msg#*missing-mapfile.pal\" not found.}" != "${msg}" || {
    echo "not ok" 1 - "missing mapfile path-not-found diagnostic"
    exit 0
}

echo "ok" 1 - "missing mapfile path is rejected"

exit 0
