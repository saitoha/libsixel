#!/bin/sh
# TAP test verifying distance-1 typo is corrected or rejected with expected message.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d burkez "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null 2>&1) || {
    case "${msg}" in
        *'specified diffusion method is not supported.'*)
            echo "ok" 1 - "distance-1 typo rejected with diagnostic"
            exit 0
            ;;
        *)
            echo "not ok" 1 - "unexpected rejection without diagnostic"
            printf '%s\n' '--- stderr ---' >&2
            printf '%s\n' "${msg}" >&2
            exit 0
            ;;
    esac
}

case "${msg}" in
    *'corrected --diffusion value "burkez" -> "burkes".'*)
        echo "ok" 1 - "distance-1 typo is corrected"
        exit 0
        ;;
    *)
        echo "not ok" 1 - "missing correction notice"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac
