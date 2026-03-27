#!/bin/sh
# TAP test verifying path suggestions are emitted for missing files.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_OPTION_PATH_SUGGESTIONS=1 \
              -m "${TOP_SRCDIR}/tests/data/inputs/snake_64.pgn" \
              "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "missing mapfile unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *'path "'*)
        ;;
    *)
        echo "not ok" 1 - "missing path suggestion diagnostics"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

case "${msg}" in
    *'Suggestions:'*)
        ;;
    *)
        echo "not ok" 1 - "missing mapfile reports unsupported suggestion lookup"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "missing mapfile prints ranked path suggestions"
exit 0
