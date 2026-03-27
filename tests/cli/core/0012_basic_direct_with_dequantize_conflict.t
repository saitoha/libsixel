#!/bin/sh
# TAP test ensuring sixel2png rejects mixing direct output with dequantize flags.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D -dk_undither <"${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
        2>&1) && {
    echo "not ok" 1 - "accepts conflicting direct/dequantize flags"
    exit 0
}

case "${msg}" in
    *"cannot be combined"*)
        ;;
    *)
        echo "not ok" 1 - "missing direct/dequantize diagnostic"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "rejects direct/dequantize mix"
exit 0
