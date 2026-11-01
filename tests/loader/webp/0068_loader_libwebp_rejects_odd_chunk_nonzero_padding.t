#!/bin/sh
# TAP test confirming forced libwebp loader rejects odd-sized chunks with non-zero padding.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! \
             "${TOP_SRCDIR}/tests/data/corrupted/bad_odd_chunk_nonzero_padding.webp" \
             2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced libwebp loader accepted odd chunk with non-zero padding"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"webp decode: odd-sized chunk has non-zero padding byte."*)
        ;;
    *)
        echo "not ok" 1 - "expected odd-chunk non-zero-padding diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "forced libwebp loader rejects odd chunk with non-zero padding"

exit 0
