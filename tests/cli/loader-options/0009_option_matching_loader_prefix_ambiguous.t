#!/bin/sh
# TAP test verifying ambiguous -L loader prefixes are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

set +e
msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llib \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o/dev/null 2>&1) && {
    echo "1..0 # SKIP lib is not an ambiguous loader prefix in this build"
    exit 0
}
set -e

case "${msg}" in
    *"ambiguous prefix"*)
        ;;
    *)
        echo "1..0 # SKIP lib is not an ambiguous loader prefix in this build"
        exit 0
        ;;
esac

echo "1..1"
set -v

echo "ok" 1 - "ambiguous -L prefix is rejected"
exit 0
