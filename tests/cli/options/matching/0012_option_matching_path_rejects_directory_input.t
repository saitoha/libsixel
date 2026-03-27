#!/bin/sh
# TAP test verifying directory arguments are rejected for file-only options.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${TOP_SRCDIR}/tests/data/inputs" \
              "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
              -o/dev/null 2>&1) && {
    echo "not ok" 1 - "directory mapfile unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *'path refers to a directory; expected a file input.'*)
        ;;
    *)
        echo "not ok" 1 - "directory rejection diagnostic was not emitted"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "directory arguments are rejected for file-only options"
exit 0
