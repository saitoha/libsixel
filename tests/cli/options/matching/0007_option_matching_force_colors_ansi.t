#!/bin/sh
# TAP test verifying forced color mode injects ANSI sequences in diagnostics.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

esc_char=$(printf '\033')

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_STATUS_FORCE_COLORS=1 \
    -d st "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "force colors diagnostic unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"${esc_char}[33m"*)
        ;;
    *)
    echo "not ok" 1 - "force colors did not inject ANSI markers"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
    ;;
esac

case "${msg}" in
    *"${esc_char}[1m"*)
        ;;
    *)
    echo "not ok" 1 - "force colors did not inject ANSI markers"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
    ;;
esac

case "${msg}" in
    *"${esc_char}[0m"*)
        ;;
    *)
    echo "not ok" 1 - "force colors did not inject ANSI markers"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
    ;;
esac

echo "ok" 1 - "force colors injects ANSI markers"
exit 0
