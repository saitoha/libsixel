#!/bin/sh
# TAP test verifying path suggestions report when no nearby entries exist.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

missing_path="${TOP_SRCDIR}/tests/quant/no-such-file.gpl"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_OPTION_PATH_SUGGESTIONS=1 \
              -m "${missing_path}" \
              "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "missing mapfile unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *'No nearby matches were found in'*)
        ;;
    *)
        echo "not ok" 1 - "missing no-nearby-matches diagnostic"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "missing path reports unsupported suggestion lookup"
exit 0
