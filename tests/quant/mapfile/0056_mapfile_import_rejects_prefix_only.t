#!/bin/sh
# TAP test: prefix-only mapfile paths are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m act: "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "prefix-only mapfile unexpectedly succeeded"
    exit 0
}

test "${msg#*path \"act:\" not found.}" != "${msg}" || {
    echo "not ok" 1 - "missing prefix-only mapfile diagnostic"
    exit 0
}

echo "ok" 1 - "prefix-only mapfile is rejected"

exit 0
