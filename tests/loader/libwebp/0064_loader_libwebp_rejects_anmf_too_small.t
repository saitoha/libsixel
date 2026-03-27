#!/bin/sh
# TAP test confirming forced libwebp loader rejects ANMF chunks smaller than 16 bytes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

msg=$(set +xv; run_img2sixel -L libwebp! \
             "${TOP_SRCDIR}/tests/data/corrupted/bad_anmf_chunk_too_small.webp" \
             2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced libwebp loader accepted too-small ANMF chunk"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"webp decode: ANMF chunk size is too small."*)
        ;;
    *)
        echo "not ok" 1 - "expected ANMF-too-small diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "forced libwebp loader rejects too-small ANMF chunk"

exit 0
